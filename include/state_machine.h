#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Enumeração dos estados possíveis da máquina de estados principal
 */
typedef enum {
    STATE_INIT = 0,      /**< Estado de inicialização e testes */
    STATE_NORMAL = 1,    /**< Estado de operação contínua */
    STATE_ALERT = 2,     /**< Estado de condições fora dos limites */
    STATE_ERROR = 3      /**< Estado de falha de sensor ou sistema */
} system_state_t;

/**
 * @brief Enumeração dos eventos que causam transições de estado
 */
typedef enum {
    EVENT_INIT_COMPLETE = 0,       /**< Inicialização completada com sucesso */
    EVENT_INIT_FAILED = 1,         /**< Inicialização falhou */
    EVENT_SENSOR_NORMAL = 2,       /**< Sensores voltaram ao normal */
    EVENT_SENSOR_OUT_OF_RANGE = 3, /**< Sensores fora dos limites */
    EVENT_SYSTEM_ERROR = 4,        /**< Erro de sistema detectado */
    EVENT_SENSOR_FAILURE = 5,      /**< Falha de sensor detectada */
    EVENT_RECOVERY = 6,            /**< Sistema se recuperou do erro */
    EVENT_RESET = 7                /**< Reset do sistema solicitado */
} system_event_t;

/**
 * @brief Tipo para callbacks de transição de estado
 * @param from_state Estado anterior
 * @param to_state Estado novo
 * @param event Evento que causou a transição
 */
typedef void (*state_transition_callback_t)(system_state_t from_state, 
                                           system_state_t to_state, 
                                           system_event_t event);

/**
 * @brief Tipo para callbacks de entrada em um estado
 * @param state Estado que foi ativado
 */
typedef void (*state_entry_callback_t)(system_state_t state);

/**
 * @brief Tipo para callbacks de saída de um estado
 * @param state Estado que foi desativado
 */
typedef void (*state_exit_callback_t)(system_state_t state);

/**
 * @brief Inicializa a máquina de estados
 * 
 * Inicia no estado STATE_INIT
 * 
 * @return true Se inicialização foi bem-sucedida
 * @return false Se inicialização falhou
 */
bool state_machine_init(void);

/**
 * @brief Registra um callback para transições de estado
 * 
 * @param callback Função a ser chamada quando houver transição
 * @return true Se registro foi bem-sucedido
 * @return false Se falhou (máximo de callbacks atingido)
 */
bool state_machine_register_transition_callback(state_transition_callback_t callback);

/**
 * @brief Registra um callback para entrada em um estado
 * 
 * @param state Estado de interesse
 * @param callback Função a ser chamada ao entrar no estado
 * @return true Se registro foi bem-sucedido
 * @return false Se falhou
 */
bool state_machine_register_entry_callback(system_state_t state, state_entry_callback_t callback);

/**
 * @brief Registra um callback para saída de um estado
 * 
 * @param state Estado de interesse
 * @param callback Função a ser chamada ao sair do estado
 * @return true Se registro foi bem-sucedido
 * @return false Se falhou
 */
bool state_machine_register_exit_callback(system_state_t state, state_exit_callback_t callback);

/**
 * @brief Envia um evento para a máquina de estados
 * 
 * Processa o evento e realiza transição se válida
 * 
 * @param event Evento a processar
 * @return true Se evento foi processado com sucesso
 * @return false Se evento não é válido no estado atual
 */
bool state_machine_process_event(system_event_t event);

/**
 * @brief Obtém o estado atual da máquina
 * 
 * @return system_state_t Estado atual
 */
system_state_t state_machine_get_current_state(void);

/**
 * @brief Obtém o nome do estado como string
 * 
 * @param state Estado
 * @return const char* Nome do estado
 */
const char* state_machine_get_state_name(system_state_t state);

/**
 * @brief Obtém o nome do evento como string
 * 
 * @param event Evento
 * @return const char* Nome do evento
 */
const char* state_machine_get_event_name(system_event_t event);

/**
 * @brief Verifica se está no estado INIT
 * 
 * @return true Se em STATE_INIT
 * @return false Caso contrário
 */
bool state_machine_is_init(void);

/**
 * @brief Verifica se está no estado NORMAL
 * 
 * @return true Se em STATE_NORMAL
 * @return false Caso contrário
 */
bool state_machine_is_normal(void);

/**
 * @brief Verifica se está no estado ALERT
 * 
 * @return true Se em STATE_ALERT
 * @return false Caso contrário
 */
bool state_machine_is_alert(void);

/**
 * @brief Verifica se está no estado ERROR
 * 
 * @return true Se em STATE_ERROR
 * @return false Caso contrário
 */
bool state_machine_is_error(void);

/**
 * @brief Obtém o número de transições realizadas
 * 
 * @return uint32_t Contador de transições
 */
uint32_t state_machine_get_transition_count(void);

/**
 * @brief Obtém o tempo em ms que passou no estado atual
 * 
 * @return uint32_t Tempo em milissegundos
 */
uint32_t state_machine_get_state_duration(void);

/**
 * @brief Reseta a máquina de estados (volta ao INIT)
 * 
 * @return true Se reset foi bem-sucedido
 * @return false Se reset falhou
 */
bool state_machine_reset(void);

/**
 * @brief Imprime informações de debug da máquina de estados
 */
void state_machine_print_debug_info(void);

#endif // STATE_MACHINE_H
