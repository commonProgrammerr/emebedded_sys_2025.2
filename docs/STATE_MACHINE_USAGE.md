/**
 * @file STATE_MACHINE_USAGE.md
 * @brief Documentação de uso da máquina de estados
 */

# Máquina de Estados - Documentação

## Visão Geral

A máquina de estados gerencia os 4 estados principais do sistema:

```
┌───────────────────────────────────────────────────────────────┐
│                    MÁQUINA DE ESTADOS                         │
│                                                               │
│  INIT ──[INIT_COMPLETE]──> NORMAL ──[SENSOR_OUT_OF_RANGE]──> ALERT
│   │         │                │ ▲          │                  │ │
│   │         └──[INIT_FAILED]─┘ │          │                  │ │
│   │                            │          │                  │ │
│   └────────[SYSTEM_ERROR]──────┴──[RECOVERY]──────────────────┘
│            [SENSOR_FAILURE]
│                            [SENSOR_NORMAL]
│
│  EVENT_RESET: Qualquer estado retorna para INIT
│
└───────────────────────────────────────────────────────────────┘
```

## Estados

### 1. STATE_INIT (Inicialização)
- **Descrição**: Estado inicial do sistema
- **Ações**: Inicializa hardware, sensores e componentes
- **Transições válidas**:
  - `EVENT_INIT_COMPLETE` → `STATE_NORMAL`
  - `EVENT_INIT_FAILED` → `STATE_ERROR`
  - `EVENT_SYSTEM_ERROR` → `STATE_ERROR`

### 2. STATE_NORMAL (Operação Normal)
- **Descrição**: Sistema operando dentro dos parâmetros normais
- **Ações**: Leitura de sensores, processamento de dados
- **Transições válidas**:
  - `EVENT_SENSOR_OUT_OF_RANGE` → `STATE_ALERT`
  - `EVENT_SYSTEM_ERROR` → `STATE_ERROR`
  - `EVENT_SENSOR_FAILURE` → `STATE_ERROR`

### 3. STATE_ALERT (Alerta)
- **Descrição**: Condições fora dos limites, mas sistema operacional
- **Ações**: Ativa buzzer/LEDs de alerta, logging intensivo
- **Transições válidas**:
  - `EVENT_SENSOR_NORMAL` → `STATE_NORMAL`
  - `EVENT_SYSTEM_ERROR` → `STATE_ERROR`
  - `EVENT_SENSOR_FAILURE` → `STATE_ERROR`

### 4. STATE_ERROR (Erro)
- **Descrição**: Falha crítica no sistema ou sensor
- **Ações**: Para operações, ativa alarmes, espera recuperação
- **Transições válidas**:
  - `EVENT_RECOVERY` → `STATE_NORMAL`
  - `EVENT_RESET` → `STATE_INIT`

## Eventos

| Evento | Descrição | Origem |
|--------|-----------|--------|
| `EVENT_INIT_COMPLETE` | Inicialização bem-sucedida | task_init |
| `EVENT_INIT_FAILED` | Falha na inicialização | task_init |
| `EVENT_SENSOR_NORMAL` | Sensores retornaram ao normal | task_sensor_monitoring |
| `EVENT_SENSOR_OUT_OF_RANGE` | Sensores lendo valores fora do range | task_sensor_monitoring |
| `EVENT_SYSTEM_ERROR` | Erro geral do sistema | task_sensor_logic |
| `EVENT_SENSOR_FAILURE` | Falha ao ler um sensor | task_sensor_logic |
| `EVENT_RECOVERY` | Sistema se recuperou do erro | task_sensor_logic |
| `EVENT_RESET` | Reset solicitado | task_alerts/external |

## Exemplo de Uso

### Inicialização

```c
#include "state_machine.h"

// Na main.c
void app_main() {
    // Inicializa máquina de estados
    state_machine_init();
    
    // Registra callbacks
    state_machine_register_transition_callback(on_state_transition);
    state_machine_register_entry_callback(STATE_ALERT, on_alert_entry);
    state_machine_register_exit_callback(STATE_ALERT, on_alert_exit);
    
    // Processa evento de inicialização bem-sucedida
    state_machine_process_event(EVENT_INIT_COMPLETE);
    // Máquina agora está em STATE_NORMAL
}
```

### Callbacks

```c
// Callback global de transição
void on_state_transition(system_state_t from_state,
                        system_state_t to_state,
                        system_event_t event) {
    ESP_LOGI("APP", "Transição: %s -> %s (evento: %s)",
             state_machine_get_state_name(from_state),
             state_machine_get_state_name(to_state),
             state_machine_get_event_name(event));
}

// Callback de entrada no estado ALERT
void on_alert_entry(system_state_t state) {
    ESP_LOGW("APP", "Sistema em ALERTA!");
    gpio_set_level(PIN_LED_YELLOW, 1);  // Acende LED amarelo
    buzzer_set_frequency(1000);          // Buzzer a 1kHz
}

// Callback de saída do estado ALERT
void on_alert_exit(system_state_t state) {
    ESP_LOGI("APP", "Alerta resolvido");
    gpio_set_level(PIN_LED_YELLOW, 0);  // Apaga LED amarelo
    buzzer_set_frequency(0);             // Desliga buzzer
}
```

### Processamento de Eventos

```c
// Na task de monitoramento de sensores
void vTaskSensorMonitoring(void *pvParameters) {
    while (1) {
        // Lê sensores
        float temperature = dht11_read_temperature();
        
        // Verifica limites
        if (temperature > MAX_TEMP || temperature < MIN_TEMP) {
            // Envia evento de alerta
            state_machine_process_event(EVENT_SENSOR_OUT_OF_RANGE);
        } else if (state_machine_is_alert()) {
            // Se estava em alerta, volta ao normal
            state_machine_process_event(EVENT_SENSOR_NORMAL);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### Verificação de Estado

```c
// Em qualquer lugar do código
if (state_machine_is_normal()) {
    // Realiza operação normal
} else if (state_machine_is_alert()) {
    // Operação em modo alerta
    buzzer_beep(3);  // Bips de alerta
} else if (state_machine_is_error()) {
    // Modo erro - operações limitadas
    stop_sensor_readings();
}
```

### Debug

```c
// Imprime informações completas
state_machine_print_debug_info();

// Saída esperada:
// === STATE MACHINE DEBUG INFO ===
// Estado Atual: NORMAL
// Total de Transições: 3
// Tempo no estado atual: 12500 ms
// Callbacks de Transição Registrados: 1
// Estado INIT - Entry callbacks: 1, Exit callbacks: 1
// Estado NORMAL - Entry callbacks: 2, Exit callbacks: 1
// Estado ALERT - Entry callbacks: 1, Exit callbacks: 1
// Estado ERROR - Entry callbacks: 0, Exit callbacks: 0
// === REGRAS DE TRANSIÇÃO VÁLIDAS ===
// INIT --[INIT_COMPLETE]--> NORMAL
// INIT --[INIT_FAILED]--> ERROR
// ...
```

## Fluxo de Execução Típico

1. **Boot**
   ```
   STATE_INIT
   ├─ Entry callback: inicializa hardware
   ├─ Processa inicialização de sensores
   └─ Quando pronto: EVENT_INIT_COMPLETE
   ```

2. **Operação Normal**
   ```
   STATE_NORMAL
   ├─ Lê sensores continuamente
   ├─ Processa dados
   └─ Se sensores fora de range: EVENT_SENSOR_OUT_OF_RANGE
   ```

3. **Alerta**
   ```
   STATE_ALERT
   ├─ Entry callback: ativa buzzer/LEDs
   ├─ Logging intensivo
   └─ Quando normaliza: EVENT_SENSOR_NORMAL → STATE_NORMAL
   ```

4. **Erro Crítico**
   ```
   STATE_ERROR
   ├─ Entry callback: para operações
   ├─ Aguarda recuperação manual
   └─ Opções: EVENT_RECOVERY ou EVENT_RESET
   ```

## Recomendações de Integração

1. **Inicializar cedo**
   ```c
   void app_main() {
       state_machine_init();
       // ... resto da inicialização
   }
   ```

2. **Registrar callbacks relevantes**
   - Use para ativar/desativar LEDs de status
   - Log de transições para debugging
   - Envio de notificações/telemetria

3. **Processar eventos de forma segura**
   - Thread-safe: usa mutex internamente
   - Verifique retorno de `state_machine_process_event()`
   - Documente quando enviar cada evento

4. **Usar verificadores de estado**
   - `state_machine_is_normal()` em loops de sensores
   - `state_machine_is_alert()` para operações de alerta
   - `state_machine_is_error()` para operações de erro

5. **Debug e Monitoramento**
   - Chame `state_machine_print_debug_info()` periodicamente
   - Implemente logging de transições
   - Considere enviar estado via UART/JSON

## Thread Safety

A máquina de estados é completamente thread-safe:
- Usa FreeRTOS Sempaphore mutex
- Timeout de 100ms para operações
- Múltiplas tasks podem processar eventos simultaneamente

## Performance

- Transições O(n) onde n = número de regras (11 total)
- Chamada de callbacks O(m) onde m = callbacks registrados
- Uso de memória: ~800 bytes (estrutura + buffers de callbacks)
