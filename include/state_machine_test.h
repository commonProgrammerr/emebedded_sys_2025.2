/**
 * @file state_machine_test.h
 * @brief Exemplos de teste e integração da máquina de estados
 */

#ifndef STATE_MACHINE_TEST_H
#define STATE_MACHINE_TEST_H

#include "state_machine.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG_TEST = "STATE_TEST";

/**
 * @brief Callback de transição de estado para logging
 */
static void state_transition_callback(system_state_t from_state,
                                     system_state_t to_state,
                                     system_event_t event) {
    ESP_LOGI(TAG_TEST, "=== TRANSIÇÃO DE ESTADO ===");
    ESP_LOGI(TAG_TEST, "De: %s", state_machine_get_state_name(from_state));
    ESP_LOGI(TAG_TEST, "Para: %s", state_machine_get_state_name(to_state));
    ESP_LOGI(TAG_TEST, "Evento: %s", state_machine_get_event_name(event));
    ESP_LOGI(TAG_TEST, "========================");
}

/**
 * @brief Callback de entrada no estado INIT
 */
static void on_init_entry(system_state_t state) {
    ESP_LOGI(TAG_TEST, ">>> Entrando em INIT");
    ESP_LOGI(TAG_TEST, "    - Inicializando hardware");
    ESP_LOGI(TAG_TEST, "    - Testando sensores");
    ESP_LOGI(TAG_TEST, "    - Calibrando periféricos");
}

/**
 * @brief Callback de saída do estado INIT
 */
static void on_init_exit(system_state_t state) {
    ESP_LOGI(TAG_TEST, "<<< Saindo de INIT");
}

/**
 * @brief Callback de entrada no estado NORMAL
 */
static void on_normal_entry(system_state_t state) {
    ESP_LOGI(TAG_TEST, ">>> Entrando em NORMAL");
    ESP_LOGI(TAG_TEST, "    - Sistema operacional");
    ESP_LOGI(TAG_TEST, "    - LEDs: Verde aceso");
    ESP_LOGI(TAG_TEST, "    - Iniciando monitoramento contínuo");
}

/**
 * @brief Callback de saída do estado NORMAL
 */
static void on_normal_exit(system_state_t state) {
    ESP_LOGI(TAG_TEST, "<<< Saindo de NORMAL");
}

/**
 * @brief Callback de entrada no estado ALERT
 */
static void on_alert_entry(system_state_t state) {
    ESP_LOGI(TAG_TEST, ">>> Entrando em ALERT");
    ESP_LOGI(TAG_TEST, "    - AVISO: Parâmetros fora dos limites!");
    ESP_LOGI(TAG_TEST, "    - LEDs: Amarelo aceso");
    ESP_LOGI(TAG_TEST, "    - Buzzer: Ativado (1kHz)");
    ESP_LOGI(TAG_TEST, "    - Logging intensivo iniciado");
}

/**
 * @brief Callback de saída do estado ALERT
 */
static void on_alert_exit(system_state_t state) {
    ESP_LOGI(TAG_TEST, "<<< Saindo de ALERT");
    ESP_LOGI(TAG_TEST, "    - Desativando buzzer");
    ESP_LOGI(TAG_TEST, "    - LEDs: Amarelo apagado");
}

/**
 * @brief Callback de entrada no estado ERROR
 */
static void on_error_entry(system_state_t state) {
    ESP_LOGE(TAG_TEST, ">>> Entrando em ERROR");
    ESP_LOGE(TAG_TEST, "    - ERRO CRÍTICO DETECTADO!");
    ESP_LOGE(TAG_TEST, "    - LEDs: Vermelho aceso");
    ESP_LOGE(TAG_TEST, "    - Buzzer: Ativado (constante)");
    ESP_LOGE(TAG_TEST, "    - Sistema aguardando recuperação");
}

/**
 * @brief Callback de saída do estado ERROR
 */
static void on_error_exit(system_state_t state) {
    ESP_LOGE(TAG_TEST, "<<< Saindo de ERROR");
    ESP_LOGE(TAG_TEST, "    - Sistema em recuperação");
}

/**
 * @brief Inicializa e registra todos os callbacks de teste
 */
static void state_machine_setup_callbacks(void) {
    // Callback global
    state_machine_register_transition_callback(state_transition_callback);
    
    // Callbacks de INIT
    state_machine_register_entry_callback(STATE_INIT, on_init_entry);
    state_machine_register_exit_callback(STATE_INIT, on_init_exit);
    
    // Callbacks de NORMAL
    state_machine_register_entry_callback(STATE_NORMAL, on_normal_entry);
    state_machine_register_exit_callback(STATE_NORMAL, on_normal_exit);
    
    // Callbacks de ALERT
    state_machine_register_entry_callback(STATE_ALERT, on_alert_entry);
    state_machine_register_exit_callback(STATE_ALERT, on_alert_exit);
    
    // Callbacks de ERROR
    state_machine_register_entry_callback(STATE_ERROR, on_error_entry);
    state_machine_register_exit_callback(STATE_ERROR, on_error_exit);
    
    ESP_LOGI(TAG_TEST, "Callbacks de teste registrados");
}

/**
 * @brief Simula cenário 1: Boot normal
 */
static void test_scenario_1_normal_boot(void) {
    ESP_LOGI(TAG_TEST, "\n===== CENÁRIO 1: BOOT NORMAL =====");
    
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG_TEST, "Processando EVENT_INIT_COMPLETE...");
    state_machine_process_event(EVENT_INIT_COMPLETE);
    
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG_TEST, "Estado final: %s", 
             state_machine_get_state_name(state_machine_get_current_state()));
}

/**
 * @brief Simula cenário 2: Alerta de sensor
 */
static void test_scenario_2_sensor_alert(void) {
    ESP_LOGI(TAG_TEST, "\n===== CENÁRIO 2: SENSOR FORA DE RANGE =====");
    
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG_TEST, "Processando EVENT_SENSOR_OUT_OF_RANGE...");
    state_machine_process_event(EVENT_SENSOR_OUT_OF_RANGE);
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG_TEST, "Estado atual: %s (duração: %ld ms)", 
             state_machine_get_state_name(state_machine_get_current_state()),
             state_machine_get_state_duration());
    
    ESP_LOGI(TAG_TEST, "Sensor normalizado, processando EVENT_SENSOR_NORMAL...");
    vTaskDelay(pdMS_TO_TICKS(500));
    state_machine_process_event(EVENT_SENSOR_NORMAL);
    
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG_TEST, "Estado final: %s", 
             state_machine_get_state_name(state_machine_get_current_state()));
}

/**
 * @brief Simula cenário 3: Erro de sensor
 */
static void test_scenario_3_sensor_error(void) {
    ESP_LOGI(TAG_TEST, "\n===== CENÁRIO 3: ERRO DE SENSOR =====");
    
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG_TEST, "Processando EVENT_SENSOR_FAILURE...");
    state_machine_process_event(EVENT_SENSOR_FAILURE);
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG_TEST, "Estado atual: %s", 
             state_machine_get_state_name(state_machine_get_current_state()));
    
    ESP_LOGI(TAG_TEST, "Sistema se recuperando, processando EVENT_RECOVERY...");
    vTaskDelay(pdMS_TO_TICKS(500));
    state_machine_process_event(EVENT_RECOVERY);
    
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG_TEST, "Estado final: %s", 
             state_machine_get_state_name(state_machine_get_current_state()));
}

/**
 * @brief Simula cenário 4: Reset do sistema
 */
static void test_scenario_4_system_reset(void) {
    ESP_LOGI(TAG_TEST, "\n===== CENÁRIO 4: RESET DO SISTEMA =====");
    
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG_TEST, "Estado antes do reset: %s", 
             state_machine_get_state_name(state_machine_get_current_state()));
    
    ESP_LOGI(TAG_TEST, "Processando EVENT_RESET...");
    state_machine_reset();
    
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG_TEST, "Estado após reset: %s", 
             state_machine_get_state_name(state_machine_get_current_state()));
}

/**
 * @brief Simula cenário 5: Transição inválida
 */
static void test_scenario_5_invalid_transition(void) {
    ESP_LOGI(TAG_TEST, "\n===== CENÁRIO 5: TRANSIÇÃO INVÁLIDA =====");
    
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG_TEST, "Estado atual: %s", 
             state_machine_get_state_name(state_machine_get_current_state()));
    
    ESP_LOGI(TAG_TEST, "Tentando transição inválida: NORMAL --[INIT_COMPLETE]-->");
    bool result = state_machine_process_event(EVENT_INIT_COMPLETE);
    ESP_LOGI(TAG_TEST, "Resultado: %s", result ? "Sucesso" : "Falhou (esperado)");
    
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP_LOGI(TAG_TEST, "Estado final: %s (inalterado)", 
             state_machine_get_state_name(state_machine_get_current_state()));
}

/**
 * @brief Task para executar todos os testes
 */
static void vTaskStateMachineTest(void *pvParameters) {
    ESP_LOGI(TAG_TEST, "Iniciando testes da máquina de estados...\n");
    
    // Inicializa máquina de estados
    state_machine_init();
    state_machine_setup_callbacks();
    
    // Executa testes
    test_scenario_1_normal_boot();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    test_scenario_2_sensor_alert();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    test_scenario_3_sensor_error();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    test_scenario_4_system_reset();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    test_scenario_5_invalid_transition();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Imprime debug final
    ESP_LOGI(TAG_TEST, "\n===== RESUMO FINAL =====");
    state_machine_print_debug_info();
    
    ESP_LOGI(TAG_TEST, "\nTestes completados!");
    ESP_LOGI(TAG_TEST, "Total de transições: %ld", state_machine_get_transition_count());
    
    vTaskDelete(NULL);
}

#endif // STATE_MACHINE_TEST_H
