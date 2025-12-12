#include "alerts.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "env.h"

static const char *TAG = "ALERT";
static TaskHandle_t xAlertTaskHandle = NULL;
static bool snooze_active = false;
static const char* alerts_tags[3] = {"NONE", "WARNING", "CRITICAL"};
static uint32_t current_snooze_duration = 0;
alert_t alert_status;

static void set_buzzer(bool on);
static void task_alert(void* args);

static void task_alert(void* args) {
    for(;;) {
        if (xTaskNotifyWait(0, 0, NULL, portMAX_DELAY) == pdTRUE) {
            switch (alert_status)
            {
            case ALERT_NONE:
                gpio_set_level(WARNING_STATE_GPIO, 0);
                gpio_set_level(CRITICAL_STATE_GPIO, 0);
                set_buzzer(false);
                break;
            case ALERT_WARNING:
                gpio_set_level(WARNING_STATE_GPIO, 1);
                gpio_set_level(CRITICAL_STATE_GPIO, 0);
                set_buzzer(false);
                break;
            case ALERT_CRITICAL:
                gpio_set_level(WARNING_STATE_GPIO, 0);
                gpio_set_level(CRITICAL_STATE_GPIO, 1);
                while (alert_status == ALERT_CRITICAL)
                {
                    set_buzzer(true);
                    BaseType_t abort = xTaskNotifyWait(0, 0, NULL, pdMS_TO_TICKS(500));
                    set_buzzer(false);
                    
                    if (abort == pdTRUE)
                        break;
                    
                    abort = xTaskNotifyWait(0, 0, NULL, pdMS_TO_TICKS(20000));
                    
                    if (abort == pdTRUE)
                        break;
                    
                    if(snooze_active) {
                        abort = xTaskNotifyWait(0, 0, NULL, pdMS_TO_TICKS(current_snooze_duration));
                        current_snooze_duration = 0;
                        if (abort == pdTRUE)
                            break;
                    }
                }
                break;
            default:
                break;
            }
        }
    }
}

static void set_buzzer(bool on) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL, on ? 1024 : 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL);
}


esp_err_t alerts_init() {
    gpio_reset_pin(WARNING_STATE_GPIO);
    gpio_reset_pin(CRITICAL_STATE_GPIO);
    gpio_set_direction(WARNING_STATE_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(CRITICAL_STATE_GPIO, GPIO_MODE_OUTPUT);

    // Config Buzzer PWM
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = BUZZER_TIMER,
        .duty_resolution = LEDC_TIMER_12_BIT,
        .freq_hz = 800,
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
    alert_status = ALERT_NONE;

    xTaskCreate(task_alert, "AlertWatchdog", 4096, NULL, 5, &xAlertTaskHandle);
    return ESP_OK;
}

esp_err_t alerts_send_alert(alert_t type, const char* message) {
    if (type == ALERT_NONE) {
        return ESP_ERR_INVALID_ARG;
    }

    alert_status = type;
    ESP_LOGW(TAG, "Novo alerta (%s)  %s", alerts_tags[type], message);
    if (xAlertTaskHandle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xTaskNotifyGive(xAlertTaskHandle);
    return ESP_OK;
}

esp_err_t alerts_clear_alert() {
    alert_status = ALERT_NONE;
    snooze_active = false;
    ESP_LOGI(TAG, "Alerta limpo");
    if (xAlertTaskHandle == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    xTaskNotifyGive(xAlertTaskHandle);
    return ESP_OK;
}

esp_err_t alerts_snooze(uint32_t duration_ms) {
    if (alert_status != ALERT_CRITICAL) {
        return ESP_ERR_INVALID_STATE;
    }
    snooze_active = true;
    current_snooze_duration = duration_ms;
    ESP_LOGI(TAG, "Alerta snoozed por %u ms", duration_ms);
    return ESP_OK;
}