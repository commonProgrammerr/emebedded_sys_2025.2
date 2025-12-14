#include "button_driver.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "env.h"

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
    int64_t now_time;
    // Flag para evitar múltiplos disparos de Long Press ou Short Press na liberação
    bool long_press_handled = false;

    while (1)
    {
        TickType_t wait_time = portMAX_DELAY;

        // Se o botão já está pressionado e ainda não tratamos o Long Press,
        // precisamos acordar exatamente quando completar o tempo de Long Press.
        if (btn->is_pressed && !long_press_handled)
        {
            now_time = esp_timer_get_time() / 1000;
            int64_t elapsed = now_time - btn->press_start_time;

            if (elapsed < LONG_PRESS_MS)
            {
                // Dorme apenas o tempo restante até completar 2s (ajustando para Ticks)
                wait_time = pdMS_TO_TICKS(LONG_PRESS_MS - elapsed);
            }
            else
            {
                // Tempo já estourou, não espera nada
                wait_time = 0;
            }
        }

        // Aguarda notificação da ISR OU o timeout calculado acima
        uint32_t notified = ulTaskNotifyTake(pdTRUE, wait_time);

        // Se foi notificado (ISR), faz debounce. Se foi timeout, não precisa.
        if (notified > 0)
        {
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));
        }

        level = gpio_get_level(btn->pin);
        now_time = esp_timer_get_time() / 1000;

        // CASO 1: Detectou borda de descida (Pressionou)
        if (level == 0 && !btn->is_pressed)
        {
            btn->is_pressed = true;
            btn->press_start_time = now_time;
            long_press_handled = false; // Reseta flag
            ESP_LOGD(TAG, "Botao Pressionado (Start Timer)");
        }

        // CASO 2: Botão continua pressionado (Verifica Long Press)
        else if (btn->is_pressed)
        {

            // Se o nível subiu (1), o usuário soltou
            if (level == 1)
            {
                btn->is_pressed = false;

                // Só dispara evento Short/Normal se o Long Press NÃO foi disparado antes
                if (!long_press_handled)
                {
                    int64_t press_duration = now_time - btn->press_start_time;
                    if (press_duration >= NORMAL_PRESS_MS)
                    { // Assumindo >= 1000ms? Verifique suas macros
                        ESP_LOGD(TAG, "Normal Press detectado");
                        if (btn->callback)
                            btn->callback(btn->pin, BUTTON_PRESS_NORMAL);
                    }
                    else
                    {
                        ESP_LOGD(TAG, "Click Simples detectado");
                        if (btn->callback)
                            btn->callback(btn->pin, BUTTON_PRESS_SHORT);
                    }
                }
                // Se long_press_handled for true, ignoramos a soltura (evento já foi consumido)
            }
            // Se o nível continua 0 (pressionado) e atingimos o tempo
            else if (!long_press_handled && (now_time - btn->press_start_time >= LONG_PRESS_MS))
            {
                ESP_LOGD(TAG, "Long Press detectado (Hold)");
                long_press_handled = true; // Marca como tratado para não repetir
                if (btn->callback)
                    btn->callback(btn->pin, BUTTON_PRESS_LONG);
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
        .intr_type = GPIO_INTR_ANYEDGE // Interrompe em qualquer mudança
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