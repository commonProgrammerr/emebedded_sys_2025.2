#include "state_machine.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "STATE_MACHINE";

// Máximo de callbacks registrados
#define MAX_TRANSITION_CALLBACKS 10
#define MAX_ENTRY_CALLBACKS 5
#define MAX_EXIT_CALLBACKS 5

/**
 * @brief Estrutura interna da máquina de estados
 */
typedef struct {
    system_state_t current_state;
    uint32_t transition_count;
    uint32_t state_entry_time;
    SemaphoreHandle_t mutex;
    
    state_transition_callback_t transition_callbacks[MAX_TRANSITION_CALLBACKS];
    uint8_t transition_callbacks_count;
    
    state_entry_callback_t entry_callbacks[4][MAX_ENTRY_CALLBACKS];
    uint8_t entry_callbacks_count[4];
    
    state_exit_callback_t exit_callbacks[4][MAX_EXIT_CALLBACKS];
    uint8_t exit_callbacks_count[4];
} state_machine_t;

static state_machine_t state_machine = {0};

/**
 * @brief Tabela de transições válidas
 * 
 * Define quais transições são permitidas entre estados
 * Estado anterior -> {Estados para os quais pode transitar}
 */
typedef struct {
    system_state_t from_state;
    system_state_t to_state;
    system_event_t event;
} transition_rule_t;

// Regras de transição documentadas
static const transition_rule_t transition_rules[] = {
    // Estado INIT - Inicialização
    {STATE_INIT, STATE_NORMAL, EVENT_INIT_COMPLETE},
    {STATE_INIT, STATE_ERROR, EVENT_INIT_FAILED},
    {STATE_INIT, STATE_ERROR, EVENT_SYSTEM_ERROR},
    
    // Estado NORMAL - Operação normal
    {STATE_NORMAL, STATE_ALERT, EVENT_SENSOR_OUT_OF_RANGE},
    {STATE_NORMAL, STATE_ERROR, EVENT_SYSTEM_ERROR},
    {STATE_NORMAL, STATE_ERROR, EVENT_SENSOR_FAILURE},
    
    // Estado ALERT - Condições fora dos limites
    {STATE_ALERT, STATE_NORMAL, EVENT_SENSOR_NORMAL},
    {STATE_ALERT, STATE_ERROR, EVENT_SYSTEM_ERROR},
    {STATE_ALERT, STATE_ERROR, EVENT_SENSOR_FAILURE},
    
    // Estado ERROR - Falha de sensor ou sistema
    {STATE_ERROR, STATE_NORMAL, EVENT_RECOVERY},
    {STATE_ERROR, STATE_INIT, EVENT_RESET},
};

#define NUM_TRANSITION_RULES (sizeof(transition_rules) / sizeof(transition_rule_t))

/**
 * @brief Mapeia eventos para strings descritivas
 */
static const char* event_names[] = {
    "INIT_COMPLETE",
    "INIT_FAILED",
    "SENSOR_NORMAL",
    "SENSOR_OUT_OF_RANGE",
    "SYSTEM_ERROR",
    "SENSOR_FAILURE",
    "RECOVERY",
    "RESET"
};

/**
 * @brief Mapeia estados para strings descritivas
 */
static const char* state_names[] = {
    "INIT",
    "NORMAL",
    "ALERT",
    "ERROR"
};

/**
 * @brief Valida se uma transição é permitida
 */
static bool is_transition_valid(system_state_t from_state, 
                               system_state_t to_state, 
                               system_event_t event) {
    for (size_t i = 0; i < NUM_TRANSITION_RULES; i++) {
        if (transition_rules[i].from_state == from_state &&
            transition_rules[i].to_state == to_state &&
            transition_rules[i].event == event) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Chama todos os callbacks de transição registrados
 */
static void call_transition_callbacks(system_state_t from_state,
                                     system_state_t to_state,
                                     system_event_t event) {
    for (uint8_t i = 0; i < state_machine.transition_callbacks_count; i++) {
        if (state_machine.transition_callbacks[i] != NULL) {
            state_machine.transition_callbacks[i](from_state, to_state, event);
        }
    }
}

/**
 * @brief Chama todos os callbacks de entrada para um estado
 */
static void call_entry_callbacks(system_state_t state) {
    if (state >= 4) return;
    
    for (uint8_t i = 0; i < state_machine.entry_callbacks_count[state]; i++) {
        if (state_machine.entry_callbacks[state][i] != NULL) {
            state_machine.entry_callbacks[state][i](state);
        }
    }
}

/**
 * @brief Chama todos os callbacks de saída para um estado
 */
static void call_exit_callbacks(system_state_t state) {
    if (state >= 4) return;
    
    for (uint8_t i = 0; i < state_machine.exit_callbacks_count[state]; i++) {
        if (state_machine.exit_callbacks[state][i] != NULL) {
            state_machine.exit_callbacks[state][i](state);
        }
    }
}

/**
 * @brief Inicializa a máquina de estados
 */
bool state_machine_init(void) {
    memset(&state_machine, 0, sizeof(state_machine_t));
    
    state_machine.current_state = STATE_INIT;
    state_machine.transition_count = 0;
    state_machine.state_entry_time = xTaskGetTickCount();
    
    state_machine.mutex = xSemaphoreCreateMutex();
    if (state_machine.mutex == NULL) {
        ESP_LOGE(TAG, "Falha ao criar mutex da máquina de estados");
        return false;
    }
    
    ESP_LOGI(TAG, "Máquina de estados inicializada no estado INIT");
    return true;
}

/**
 * @brief Registra callback para transição de estado
 */
bool state_machine_register_transition_callback(state_transition_callback_t callback) {
    if (callback == NULL) {
        return false;
    }
    
    if (xSemaphoreTake(state_machine.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    
    if (state_machine.transition_callbacks_count >= MAX_TRANSITION_CALLBACKS) {
        xSemaphoreGive(state_machine.mutex);
        ESP_LOGW(TAG, "Máximo de transition callbacks atingido");
        return false;
    }
    
    state_machine.transition_callbacks[state_machine.transition_callbacks_count] = callback;
    state_machine.transition_callbacks_count++;
    
    xSemaphoreGive(state_machine.mutex);
    return true;
}

/**
 * @brief Registra callback para entrada em estado
 */
bool state_machine_register_entry_callback(system_state_t state, state_entry_callback_t callback) {
    if (callback == NULL || state >= 4) {
        return false;
    }
    
    if (xSemaphoreTake(state_machine.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    
    if (state_machine.entry_callbacks_count[state] >= MAX_ENTRY_CALLBACKS) {
        xSemaphoreGive(state_machine.mutex);
        ESP_LOGW(TAG, "Máximo de entry callbacks para estado %d atingido", state);
        return false;
    }
    
    state_machine.entry_callbacks[state][state_machine.entry_callbacks_count[state]] = callback;
    state_machine.entry_callbacks_count[state]++;
    
    xSemaphoreGive(state_machine.mutex);
    return true;
}

/**
 * @brief Registra callback para saída de estado
 */
bool state_machine_register_exit_callback(system_state_t state, state_exit_callback_t callback) {
    if (callback == NULL || state >= 4) {
        return false;
    }
    
    if (xSemaphoreTake(state_machine.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    
    if (state_machine.exit_callbacks_count[state] >= MAX_EXIT_CALLBACKS) {
        xSemaphoreGive(state_machine.mutex);
        ESP_LOGW(TAG, "Máximo de exit callbacks para estado %d atingido", state);
        return false;
    }
    
    state_machine.exit_callbacks[state][state_machine.exit_callbacks_count[state]] = callback;
    state_machine.exit_callbacks_count[state]++;
    
    xSemaphoreGive(state_machine.mutex);
    return true;
}

/**
 * @brief Processa um evento na máquina de estados
 */
bool state_machine_process_event(system_event_t event) {
    if (xSemaphoreTake(state_machine.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Timeout ao acessar mutex");
        return false;
    }
    
    system_state_t from_state = state_machine.current_state;
    system_state_t to_state = from_state;
    bool transition_valid = false;
    
    // Procura transição válida para este evento
    for (size_t i = 0; i < NUM_TRANSITION_RULES; i++) {
        if (transition_rules[i].from_state == from_state &&
            transition_rules[i].event == event) {
            to_state = transition_rules[i].to_state;
            transition_valid = true;
            break;
        }
    }
    
    if (!transition_valid) {
        ESP_LOGW(TAG, "Transição inválida: %s --[%s]--> Não definida",
                 state_names[from_state], event_names[event]);
        xSemaphoreGive(state_machine.mutex);
        return false;
    }
    
    // Se não mudou de estado, apenas retorna
    if (from_state == to_state) {
        xSemaphoreGive(state_machine.mutex);
        return true;
    }
    
    // Chama callback de saída
    call_exit_callbacks(from_state);
    
    // Realiza a transição
    state_machine.current_state = to_state;
    state_machine.transition_count++;
    state_machine.state_entry_time = xTaskGetTickCount();
    
    // Chama callbacks de transição
    call_transition_callbacks(from_state, to_state, event);
    
    // Chama callback de entrada
    call_entry_callbacks(to_state);
    
    ESP_LOGI(TAG, "Transição: %s --[%s]--> %s (Transição #%ld)",
             state_names[from_state], event_names[event], 
             state_names[to_state], state_machine.transition_count);
    
    xSemaphoreGive(state_machine.mutex);
    return true;
}

/**
 * @brief Obtém estado atual
 */
system_state_t state_machine_get_current_state(void) {
    system_state_t state;
    
    if (xSemaphoreTake(state_machine.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return STATE_ERROR;
    }
    
    state = state_machine.current_state;
    xSemaphoreGive(state_machine.mutex);
    
    return state;
}

/**
 * @brief Obtém nome do estado
 */
const char* state_machine_get_state_name(system_state_t state) {
    if (state >= 4) {
        return "UNKNOWN";
    }
    return state_names[state];
}

/**
 * @brief Obtém nome do evento
 */
const char* state_machine_get_event_name(system_event_t event) {
    if (event >= 8) {
        return "UNKNOWN";
    }
    return event_names[event];
}

/**
 * @brief Verifica se está em INIT
 */
bool state_machine_is_init(void) {
    return state_machine_get_current_state() == STATE_INIT;
}

/**
 * @brief Verifica se está em NORMAL
 */
bool state_machine_is_normal(void) {
    return state_machine_get_current_state() == STATE_NORMAL;
}

/**
 * @brief Verifica se está em ALERT
 */
bool state_machine_is_alert(void) {
    return state_machine_get_current_state() == STATE_ALERT;
}

/**
 * @brief Verifica se está em ERROR
 */
bool state_machine_is_error(void) {
    return state_machine_get_current_state() == STATE_ERROR;
}

/**
 * @brief Obtém número de transições
 */
uint32_t state_machine_get_transition_count(void) {
    uint32_t count;
    
    if (xSemaphoreTake(state_machine.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return 0;
    }
    
    count = state_machine.transition_count;
    xSemaphoreGive(state_machine.mutex);
    
    return count;
}

/**
 * @brief Obtém tempo no estado atual
 */
uint32_t state_machine_get_state_duration(void) {
    uint32_t duration;
    
    if (xSemaphoreTake(state_machine.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return 0;
    }
    
    uint32_t current_time = xTaskGetTickCount();
    duration = current_time - state_machine.state_entry_time;
    xSemaphoreGive(state_machine.mutex);
    
    return duration;
}

/**
 * @brief Reseta máquina de estados
 */
bool state_machine_reset(void) {
    if (xSemaphoreTake(state_machine.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    
    call_exit_callbacks(state_machine.current_state);
    
    state_machine.current_state = STATE_INIT;
    state_machine.state_entry_time = xTaskGetTickCount();
    state_machine.transition_count++;
    
    call_entry_callbacks(STATE_INIT);
    
    ESP_LOGI(TAG, "Máquina de estados resetada para INIT");
    xSemaphoreGive(state_machine.mutex);
    
    return true;
}

/**
 * @brief Imprime informações de debug
 */
void state_machine_print_debug_info(void) {
    if (xSemaphoreTake(state_machine.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Timeout ao acessar mutex para debug");
        return;
    }
    
    ESP_LOGI(TAG, "=== STATE MACHINE DEBUG INFO ===");
    ESP_LOGI(TAG, "Estado Atual: %s", state_names[state_machine.current_state]);
    ESP_LOGI(TAG, "Total de Transições: %ld", state_machine.transition_count);
    ESP_LOGI(TAG, "Tempo no estado atual: %ld ms", state_machine_get_state_duration());
    ESP_LOGI(TAG, "Callbacks de Transição Registrados: %d", state_machine.transition_callbacks_count);
    
    for (int i = 0; i < 4; i++) {
        ESP_LOGI(TAG, "Estado %s - Entry callbacks: %d, Exit callbacks: %d",
                 state_names[i],
                 state_machine.entry_callbacks_count[i],
                 state_machine.exit_callbacks_count[i]);
    }
    
    ESP_LOGI(TAG, "=== REGRAS DE TRANSIÇÃO VÁLIDAS ===");
    for (size_t i = 0; i < NUM_TRANSITION_RULES; i++) {
        ESP_LOGI(TAG, "%s --[%s]--> %s",
                 state_names[transition_rules[i].from_state],
                 event_names[transition_rules[i].event],
                 state_names[transition_rules[i].to_state]);
    }
    
    xSemaphoreGive(state_machine.mutex);
}
