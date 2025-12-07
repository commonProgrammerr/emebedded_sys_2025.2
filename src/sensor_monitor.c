#include "sensor_monitor.h"

sensor_monitor_t* sensors[MAX_SENSORS];
uint8_t sensor_count = 0;

/**
 * @brief Task dedicada para leitura de um sensor específico
 * 
 * Esta task fica bloqueada esperando por notificações do timer.
 * Quando notificada:
 * 1. Aloca memória para os dados do sensor
 * 2. Executa a leitura do sensor
 * 3. Se bem-sucedido, chama o callback (se fornecido)
 * 4. Libera a memória e volta a esperar
 * 
 * @param args Ponteiro para sensor_monitor_t
 * @note Usa ulTaskNotifyTake() para esperar notificações do timer
 */
static void sensor_read_task(void *args) {
    sensor_monitor_t* sensor = (sensor_monitor_t*)args;

    // Validação: garante que o sensor foi passado corretamente
    if (sensor == NULL) {
        ESP_LOGE(SENSOR_MONITOR_TAG, "Sensor monitor is NULL in sensor read task");
        vTaskDelete(NULL);
        return;
    }
    sensor->is_running = pdTRUE;

    while (1) {
        // Bloqueia indefinidamente até receber notificação do timer
        // pdTRUE: limpa o contador de notificações após ler
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // Aloca memória temporária para os dados do sensor
        void* data = malloc(sensor->data_size);
        
        // Executa a leitura do sensor usando a interface sensor_base
        SensorStatus_t status = sensor->sensor->read_data(sensor->sensor, &data);
        
        if (status == SENSOR_OK) {
            ESP_LOGD(SENSOR_MONITOR_TAG, "Sensor %s data read successfully", sensor->sensor_name);
            
            // Se callback foi fornecido, executa com os dados lidos
            if (sensor->data_callback) {
                sensor->data_callback(sensor->sensor, data);
            }
        } else {
            ESP_LOGE(SENSOR_MONITOR_TAG, "Failed to read data from sensor %s", sensor->sensor_name);
        }
        
        // Libera memória temporária
        free(data);
    }
}

/**
 * @brief Callback do Software Timer - executado periodicamente
 * 
 * Este callback é chamado pelo FreeRTOS quando o timer dispara.
 * Ele identifica qual sensor corresponde ao timer e notifica
 * sua task de leitura usando Task Notification.
 * 
 * FLUXO:
 * Timer dispara → callback executa → encontra sensor → notifica task
 * 
 * @param xTimer Handle do timer que disparou
 * @note Task Notification é mais eficiente que filas ou semáforos
 */
static void sensor_monitor_timer_callback(TimerHandle_t xTimer) {
    // Percorre todos os sensores registrados
    for (size_t i = 0; i < sensor_count; i++)
    {
        // Verifica se este sensor corresponde ao timer que disparou
        if(sensors[i] != NULL && sensors[i]->timer == xTimer && sensors[i]->is_running) {
            // Notifica a task do sensor para executar leitura
            // xTaskNotifyGive incrementa o contador de notificações da task
            xTaskNotifyGive(sensors[i]->task_handle);
            break; // Timer encontrado, pode sair do loop
        }
    }
}

/**
 * @brief Inicializa e inicia o Software Timer do sensor
 * 
 * Cria um timer periódico que dispara no intervalo configurado.
 * O timer executa o callback sensor_monitor_timer_callback.
 * 
 * @param sensor Ponteiro para o monitor do sensor
 */
static void init_timer(sensor_monitor_t* sensor) {
    // Verifica se já está rodando para evitar criar timer duplicado
    if(sensor->is_running) {
        ESP_LOGW(SENSOR_MONITOR_TAG, "Sensor monitoring already running");
        return;
    } else if (sensor->timer == NULL) {
        ESP_LOGD(SENSOR_MONITOR_TAG, "Creating timer for sensor monitoring");
        
        // Cria Software Timer do FreeRTOS
        sensor->timer = xTimerCreate(
            "SensorTimer",                    // Nome do timer (para debug)
            sensor->interval,                 // Período em ticks
            pdTRUE,                           // pdTRUE = auto-reload (periódico)
            (void *)sensor,                   // ID do timer (não usado aqui)
            sensor_monitor_timer_callback     // Função callback
        );
    }

    if (sensor->timer == NULL) {
        ESP_LOGE(SENSOR_MONITOR_TAG, "Failed to create timer for sensor");
        return;
    }

    // Inicia o timer (começa a contar)
    if (xTimerStart(sensor->timer, 0) != pdPASS) {
        ESP_LOGE(SENSOR_MONITOR_TAG, "Failed to start timer for sensor");
    } else {
        sensor->is_running = pdTRUE;
        ESP_LOGI(SENSOR_MONITOR_TAG, "Started monitoring sensor");
    }
}

/**
 * @brief Cria a task dedicada de leitura do sensor
 * 
 * Cada sensor tem sua própria task que fica aguardando notificações.
 * A task é criada com prioridade baixa (IDLE + 1) pois não é crítica.
 * 
 * @param sensor Ponteiro para o monitor do sensor
 */
static void init_sensor_task(sensor_monitor_t* sensor) {
    // Verifica se a task já existe
    if (sensor->task_handle != NULL) {
        ESP_LOGW(SENSOR_MONITOR_TAG, "Sensor task already running");
        return;
    }

    // Cria task do FreeRTOS
    BaseType_t result = xTaskCreate(
        sensor_read_task,           // Função da task
        sensor->sensor_name,        // Nome da task (aparece no debugger)
        2048,                       // Tamanho da stack em bytes
        (void*)sensor,              // Parâmetro passado para a task
        tskIDLE_PRIORITY + 1,       // Prioridade (baixa, mas acima de idle)
        &sensor->task_handle        // Handle retornado da task criada
    );

    if (result != pdPASS) {
        ESP_LOGE(SENSOR_MONITOR_TAG, "Failed to create sensor read task");
        sensor->task_handle = NULL;
    } else {
        ESP_LOGI(SENSOR_MONITOR_TAG, "Sensor read task created successfully");
    }
}

/**
 * @brief Cria uma nova instância de monitor de sensor
 * 
 * Aloca memória e inicializa os campos da estrutura.
 * O monitoramento não inicia automaticamente - use start_sensor_monitoring().
 * 
 * EXEMPLO:
 * sensor_monitor_t* dht_monitor = new_sensor_monitor(
 *     dht_sensor,
 *     2000,                  // Ler a cada 2 segundos
 *     sizeof(dht11_data_t),
 *     "DHT11",
 *     my_callback_function
 * );
 */
sensor_monitor_t* new_sensor_monitor(sensor_base_t* sensor_base, TickType_t interval_ms, size_t data_size, const char* sensor_name, sensor_data_callback_t callback) {
    // Aloca memória para a estrutura do monitor
    sensor_monitor_t* sensor = (sensor_monitor_t*)malloc(sizeof(sensor_monitor_t));
    if (sensor == NULL) {
        ESP_LOGE(SENSOR_MONITOR_TAG, "Failed to allocate memory for sensor monitor");
        return NULL;
    }

    // Inicializa todos os campos
    sensor->sensor = sensor_base;                      // Sensor a ser monitorado
    sensor->interval = pdMS_TO_TICKS(interval_ms);    // Converte ms para ticks do FreeRTOS
    sensor->is_running = pdFALSE;                     // Ainda não iniciado
    sensor->data_size = data_size;                    // Tamanho dos dados para malloc
    sensor->sensor_name = strdup(sensor_name);        // Copia string do nome
    sensor->data_callback = callback;                 // Callback (pode ser NULL)
    sensor->timer = NULL;                             // Timer será criado depois
    sensor->task_handle = NULL;                       // Task será criada depois

    return sensor;
}

/**
 * @brief Inicia o monitoramento periódico de um sensor
 * 
 * Registra o sensor no array global, cria a task de leitura
 * e inicia o timer periódico.
 * 
 * SEQUÊNCIA:
 * 1. Adiciona sensor ao array global
 * 2. Cria task dedicada para o sensor
 * 3. Cria e inicia timer periódico
 * 
 * @param sensor Ponteiro para o sensor_monitor_t a ser iniciado
 */
void start_sensor_monitoring(sensor_monitor_t* sensor) {
    // Verifica limite de sensores
    if (sensor_count >= MAX_SENSORS) {
        ESP_LOGE(SENSOR_MONITOR_TAG, "Maximum number of sensors reached");
        return;
    }

    // Registra sensor no array global
    sensors[sensor_count] = sensor;
    sensor_count++;

    // Inicializa timer e task
    init_timer(sensor);        // Cria e inicia timer periódico
    init_sensor_task(sensor);  // Cria task que aguarda notificações
}

/**
 * @brief Para o monitoramento de um ou todos os sensores
 * 
 * Para o timer periódico, impedindo que o callback continue
 * notificando a task. A task permanece em execução aguardando
 * notificações (que não virão mais).
 * 
 * USO:
 * stop_sensor_monitoring(monitor);  // Para um sensor específico
 * stop_sensor_monitoring(NULL);     // Para TODOS os sensores
 * 
 * @param sensor Ponteiro para sensor específico, ou NULL para parar todos
 * @note Não deleta a task nem libera memória - apenas para o timer
 */
void stop_sensor_monitoring(sensor_monitor_t* sensor) {
    // Verifica se há sensores monitorados
    if (sensor_count == 0) {
        ESP_LOGW(SENSOR_MONITOR_TAG, "No sensors are being monitored");
        return;
    }

    // Se sensor é NULL, para todos os sensores
    if (sensor == NULL) {
        ESP_LOGW(SENSOR_MONITOR_TAG, "Stopping all sensors");
        for (uint8_t i = 0; i < sensor_count; i++) {
            sensor_monitor_t* sensor = sensors[i];
            if (sensor->is_running) {
                // Para o timer do FreeRTOS
                if (xTimerStop(sensor->timer, 0) != pdPASS) {
                    ESP_LOGE(SENSOR_MONITOR_TAG, "Failed to stop timer for sensor");
                } else {
                    sensor->is_running = pdFALSE;
                    ESP_LOGI(SENSOR_MONITOR_TAG, "Stopped monitoring sensor");
                }
            }
        }
    } else {
        // Para timer de um sensor específico
        if (sensor->is_running) {
            // xTimerStop para o timer, mas não o deleta
            if (xTimerStop(sensor->timer, 0) != pdPASS) {
                ESP_LOGE(SENSOR_MONITOR_TAG, "Failed to stop timer for sensor");
            } else {
                sensor->is_running = pdFALSE;
                ESP_LOGI(SENSOR_MONITOR_TAG, "Stopped monitoring sensor");
            }
        } else {
            ESP_LOGW(SENSOR_MONITOR_TAG, "Sensor is not running");
        }
    }
}