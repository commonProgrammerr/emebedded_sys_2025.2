#include "alerts.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "env.h"

static const char *TAG = "ALERTS_CORE";

// Definição das filas
QueueHandle_t xQueueDHT = NULL;
QueueHandle_t xQueueLight = NULL;
QueueHandle_t xQueueNoise = NULL;

// Variáveis de Estado do Sistema
static bool is_night_mode = false;
static bool snooze_active = false;
static int64_t snooze_start_time = 0;

// Estados Individuais
typedef enum { STATUS_OK, STATUS_ATTENTION, STATUS_ALARM } SensorStatus_t;
static SensorStatus_t st_dht = STATUS_OK;
static SensorStatus_t st_light = STATUS_OK;
static SensorStatus_t st_noise = STATUS_OK;


static float dht_temp_buf[DHT_WINDOW] = {0};
static float dht_hum_buf[DHT_WINDOW] = {0};
static int dht_idx = 0;
static int dht_count = 0;
static int dht_violation_counter = 0; // Para regra de >= 3 leituras

static int light_violation_counter = 0; 

static int noise_peak_counter = 0; 

void init_actuators() {
    gpio_reset_pin(WARNING_STATE_GPIO);
    gpio_reset_pin(CRITICAL_STATE_GPIO);
    gpio_set_direction(WARNING_STATE_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CRITICAL_STATE_GPIO, GPIO_MODE_OUTPUT);

    // Config Buzzer PWM
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = BUZZER_TIMER,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 2000,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BUZZER_CHANNEL,
        .timer_sel = BUZZER_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = BUZZER_GPIO,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel);
}

void set_buzzer(bool on) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL, on ? 512 : 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL);
}

// --- Funções Auxiliares de Cálculo ---
float calc_avg(float *buf, int count) {
    float sum = 0;
    for(int i=0; i<count; i++) sum += buf[i];
    return (count > 0) ? sum/count : 0;
}

// --- TASK: Processamento Lógico ---
void vTaskAlertLogic(void *pvParameters) {
    dht11_t dht_data;
    float lux_data;
    uint16_t noise_data;

    for (;;) {
        // 1. Processar DHT (Temperatura/Umidade)
        if (xQueueReceive(xQueueDHT, &dht_data, 0) == pdTRUE) {
            // Adiciona ao buffer circular (Média móvel de 1 min)
            dht_temp_buf[dht_idx] = dht_data.temperature;
            dht_hum_buf[dht_idx] = dht_data.humidity;
            dht_idx = (dht_idx + 1) % DHT_WINDOW;
            if (dht_count < DHT_WINDOW) dht_count++;

            float avg_t = calc_avg(dht_temp_buf, dht_count);
            float avg_h = calc_avg(dht_hum_buf, dht_count);

            bool bad_t = (avg_t < TEMP_MIN || avg_t > TEMP_MAX);
            bool bad_h = (avg_h < HUM_MIN || avg_h > HUM_MAX);

            // Regra: >= 3 leituras consecutivas fora da faixa
            if (bad_t || bad_h) {
                dht_violation_counter++;
            } else {
                dht_violation_counter = 0; // Reset se voltar ao normal
            }

            if (dht_violation_counter >= 3) st_dht = STATUS_ALARM;
            else if (dht_violation_counter > 0) st_dht = STATUS_ATTENTION;
            else st_dht = STATUS_OK;

            ESP_LOGI(TAG, "DHT Avg T:%.1f H:%.1f | Violations: %d | State: %d", avg_t, avg_h, dht_violation_counter, st_dht);
        }

        // 2. Processar Luz
        if (xQueueReceive(xQueueLight, &lux_data, 0) == pdTRUE) {
            if (is_night_mode) {
                // Regra: Alerta se luz > 0 (tolerância 5) por > 2 min
                if (lux_data > LUX_NIGHT_MAX) light_violation_counter++;
                else light_violation_counter = 0;

                // Se amostra a cada 10s -> 2 min = 12 amostras
                if (light_violation_counter >= 12) st_light = STATUS_ALARM;
                else if (light_violation_counter > 0) st_light = STATUS_ATTENTION;
                else st_light = STATUS_OK;
            } else {
                // Modo Diurno (Regra simples: não pode estar breu)
                if (lux_data < 50) st_light = STATUS_ATTENTION;
                else st_light = STATUS_OK;
            }
        }

        // 3. Processar Ruído
        if (xQueueReceive(xQueueNoise, &noise_data, 0) == pdTRUE) {
            // Regra: Pico > 3s (assumindo leitura 1s -> 3 amostras)
            if (noise_data > NOISE_PEAK_LIMIT) noise_peak_counter++;
            else noise_peak_counter = 0;

            if (noise_peak_counter >= 3) st_noise = STATUS_ALARM;
            else if (noise_data > NOISE_AVG_LIMIT) st_noise = STATUS_ATTENTION;
            else st_noise = STATUS_OK;
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Pequeno delay para não travar a CPU
    }
}

// --- TASK: Atuadores (Feedback Visual/Sonoro) ---
void vTaskAlertActuators(void *pvParameters) {
    int tick = 0;

    for (;;) {
        // Determina o pior estado geral
        SensorStatus_t global_state = STATUS_OK;
        if (st_dht == STATUS_ALARM || st_light == STATUS_ALARM || st_noise == STATUS_ALARM) 
            global_state = STATUS_ALARM;
        else if (st_dht == STATUS_ATTENTION || st_light == STATUS_ATTENTION || st_noise == STATUS_ATTENTION) 
            global_state = STATUS_ATTENTION;

        // Reset LEDs
        gpio_set_level(WARNING_STATE_GPIO, 0);
        gpio_set_level(CRITICAL_STATE_GPIO, 0);

        // Lógica Snooze (Expira após 10 min)
        if (snooze_active && (esp_timer_get_time() - snooze_start_time > 600000000)) {
            snooze_active = false; // Snooze acabou
        }

        switch (global_state) {
            case STATUS_OK:
                gpio_set_level(WARNING_STATE_GPIO, 0);
                gpio_set_level(CRITICAL_STATE_GPIO, 0);
                set_buzzer(false);
                break;

            case STATUS_ATTENTION:
                // Pisca Amarelo
                if ((tick % 10) < 5) gpio_set_level(WARNING_STATE_GPIO, 1);
                set_buzzer(false);
                break;

            case STATUS_ALARM:
                gpio_set_level(CRITICAL_STATE_GPIO, 1);
                
                // Lógica Buzzer: Toque curto a cada 30s
                // 30s = 300 ticks de 100ms. Toque curto = 5 ticks (0.5s)
                bool buzzer_time = (tick % 300) < 5; 
                
                if (buzzer_time && !snooze_active) {
                    set_buzzer(true);
                } else {
                    set_buzzer(false);
                }
                break;
        }

        tick++;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// --- Funções Públicas ---
void alerts_init() {
    init_actuators();
    
    // Criação das filas (Tamanho 5 é suficiente pois consumimos rápido)
    xQueueDHT = xQueueCreate(5, sizeof(dht11_t));
    xQueueLight = xQueueCreate(5, sizeof(float));
    xQueueNoise = xQueueCreate(5, sizeof(uint16_t));

    xTaskCreate(vTaskAlertLogic, "AlertLogic", 4096, NULL, 5, NULL);
    xTaskCreate(vTaskAlertActuators, "AlertActuators", 2048, NULL, 5, NULL);
}

void alerts_trigger_snooze() {
    snooze_active = true;
    snooze_start_time = esp_timer_get_time();
    ESP_LOGI(TAG, "Snooze ativado por 10 min");
}

void alerts_set_night_mode(bool is_night) {
    is_night_mode = is_night;
    ESP_LOGI(TAG, "Modo Noturno: %d", is_night);
}