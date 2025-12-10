#include "button_driver.h"
#include "esp_log.h"
#include "esp_timer.h"

#define DEBOUNCE_TIME_MS 50
#define LONG_PRESS_MS 2000
#define NORMAL_PRESS_MS 500

static const char *TAG = "BUTTON_DRV";
static Button_t *btn_instance = NULL; // Para acessar na ISR

// ISR - Chamada na interrupção (IRAM_ATTR = roda na RAM rápida)
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    Button_t *btn = (Button_t *)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // Notifica a task do botão que houve uma mudança
    vTaskNotifyGiveFromISR(btn->task_to_notify, &xHigherPriorityTaskWoken);
    
    // Força troca de contexto se necessário
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Task que processa os eventos do botão
void button_task(void *pvParameters)
{
    Button_t *btn = (Button_t *)pvParameters;
    int level;
    int64_t press_time;
    
    while (1) {
        // Aguarda notificação da ISR (bloqueante)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // Debounce: aguarda estabilizar
        vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));
        
        level = gpio_get_level(btn->pin);
        
        if (level == 0 && !btn->is_pressed) {
            // Botão pressionado (transição HIGH -> LOW)
            btn->is_pressed = true;
            btn->press_start_time = esp_timer_get_time() / 1000; // em ms
            ESP_LOGI(TAG, "Botao pressionado");
            
        } else if (level == 1 && btn->is_pressed) {
            // Botão solto (transição LOW -> HIGH)
            btn->is_pressed = false;
            press_time = (esp_timer_get_time() / 1000) - btn->press_start_time;
            
            // Determina o tipo de evento
            button_event_t event;
            if (press_time >= LONG_PRESS_MS) {
                event = BUTTON_PRESS_LONG;
                ESP_LOGI(TAG, "Long Press detectado (%lld ms)", press_time);
            } else if (press_time >= NORMAL_PRESS_MS) {
                event = BUTTON_PRESS_NORMAL;
                ESP_LOGI(TAG, "Normal Press detectado (%lld ms)", press_time);
            } else {
                event = BUTTON_PRESS_SHORT;
                ESP_LOGI(TAG, "Short Press detectado (%lld ms)", press_time);
            }
            
            // Chama o callback do usuário
            if (btn->callback) {
                btn->callback(btn->pin, event);
            }
        }
    }
}

void button_init(Button_t *btn, gpio_num_t pin, button_callback_t cb, TaskHandle_t task_handle)
{
    btn->pin = pin;
    btn->callback = cb;
    btn->is_pressed = false;
    btn->press_start_time = 0;
    btn_instance = btn;
    
    // Configura GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE  // Interrompe em qualquer mudança
    };
    gpio_config(&io_conf);
    
    // Instala o serviço de interrupção GPIO
    gpio_install_isr_service(0);
    
    // Cria a task interna que processará os eventos
    TaskHandle_t button_task_handle;
    xTaskCreate(button_task, "button_task", 2048, btn, 10, &button_task_handle);
    btn->task_to_notify = button_task_handle;
    
    // Adiciona o handler da ISR
    gpio_isr_handler_add(pin, gpio_isr_handler, (void *)btn);
    
    ESP_LOGI(TAG, "Botao inicializado no GPIO %d com interrupcao", pin);
}