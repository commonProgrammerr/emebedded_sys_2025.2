# Detalhes de Implementação - Sistema de Monitoramento Ambiental

## Visão Geral Técnica

Sistema embarcado de monitoramento multi-sensor baseado em ESP32 (ESP-IDF 5.x) com FreeRTOS. Arquitetura orientada a eventos com persistência em flash NVS, processamento de sinais digitais (DSP) para análise de ruído, e sistema de alertas multi-nível com proteção thread-safe.

**Stack Tecnológico:**
- ESP-IDF 5.x (Espressif IoT Development Framework)
- FreeRTOS 10.x (kernel RTOS integrado)
- NVS (Non-Volatile Storage) para persistência
- LWIP para stack TCP/IP (WiFi + NTP)
- PlatformIO como build system

## Decisões de Arquitetura

### Pattern: Sensor Abstraction Layer
Interface `sensor_base_t` define contrato comum para todos os sensores:
```c
typedef struct {
    const char* name;
    esp_err_t (*init)(void* config);
    esp_err_t (*read)(void* data);
    esp_err_t (*deinit)(void);
} sensor_base_t;
```

**Rationale:** Permite adicionar sensores sem modificar código existente (Open/Closed Principle). Cada driver implementa a interface de forma independente.

### Pattern: Observer com Callbacks
Sensores notificam sistema através de callbacks registrados:
```c
typedef void (*sensor_callback_t)(sensor_base_t *sensor, void *data);
```

**Fluxo:**
1. Sensor monitor task executa leitura periódica via Software Timer
2. Callback notifica subsistemas interessados (histórico, alertas)
3. Decoupling: Sensores não conhecem consumidores dos dados

### Thread Safety Strategy

**Alert System Mutex:**
- Variável `alert_status` protegida por `SemaphoreHandle_t alert_mutex`
- Timeout de 1000ms para evitar deadlocks
- Retorna `ESP_ERR_TIMEOUT` se mutex não disponível
- Task `task_alert` copia estado localmente antes de processar

**MAX9814 Library Mutex:**
- Buffer de amostras protegido por mutex dedicado
- Aquisição ADC é operação atômica (oneshot mode)
- taskYIELD() a cada 128 amostras previne starvation

**NVS Flash Access:**
- Serializado por natureza (NVS library é thread-safe internamente)
- Buffer circular implementa overwrite automático quando cheio

### Memory Management

**Stack Allocation:**
- `app_main`: 4096 bytes (padrão ESP-IDF)
- `task_alert`: 4096 bytes (necessário para callback chain)
- `sensor_monitor`: 4096 bytes (handling de timeouts e callbacks)
- Timer Service: 4096 bytes (aumentado de 2048 após stack overflow)

**Heap Allocation:**
- Sensores: Alocação única em `init()`, liberação em `deinit()`
- Flash buffer: Estrutura persistente durante lifetime do sistema
- Dados temporários: Alocados em stack sempre que possível

## Arquitetura de Tasks FreeRTOS

### Task Hierarchy e Prioridades

```
Priority 5: task_alert (AlertWatchdog)
  └─ Responde a eventos de alerta via TaskNotify
  └─ Controla GPIO (LEDs) e PWM (buzzer) 
  └─ Implementa state machine ALERT_NONE → WARNING → CRITICAL

Priority 5: sensor_monitor_task (instâncias múltiplas)
  └─ DHT11 Monitor: Timer 2000ms
  └─ BH1750 Monitor: Timer 10000ms  
  └─ MAX9814 Monitor: Timer 1000ms
  └─ Notificado por Software Timer (LEDC Timer)

Priority 1: app_main (implícito)
  └─ Inicialização sequencial do sistema
  └─ Bloqueia aguardando Task Notification do botão
  └─ Executa dump de dados e reinicialização
```

**Design Decision:** Prioridade 5 para alertas garante resposta imediata a condições críticas, mesmo sob carga de sensores.

### Sincronização e Comunicação Inter-Task

**Software Timers (ESP Timer):**
```c
esp_timer_handle_t sensor_timer;
esp_timer_create_args_t timer_args = {
    .callback = sensor_timer_callback,
    .arg = monitor,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "sensor_timer"
};
```

**Task Notifications (FreeRTOS):**
- Mecanismo leve de sincronização (4 bytes por task)
- Usado em: botão → app_main, alertas → task_alert
- Alternativa a semáforos binários (menor overhead)

**Mutexes (FreeRTOS):**
- `alert_mutex`: Protege `alert_status` global
- `max9814_mutex`: Protege buffer de amostras ADC
- Timeout de 1000ms previne deadlock

### Pipeline de Processamento de Dados

```
┌─────────────┐
│ ADC/I2C/1W  │ Hardware Abstraction Layer
└──────┬──────┘
       │
┌──────▼──────────────────────────────┐
│ Sensor Drivers (DHT, BH1750, MAX)   │ Device Drivers
│  - Protocol handling                │
│  - Error recovery                   │
│  - Calibration/filtering            │
└──────┬──────────────────────────────┘
       │
┌──────▼──────────────────────────────┐
│ Sensor Monitor                      │ Orchestration Layer
│  - Periodic scheduling              │
│  - Timeout handling                 │
│  - Callback dispatch                │
└──────┬──────────────────────────────┘
       │
       ├──────────────────┬────────────────────┐
       │                  │                    │
┌──────▼──────┐  ┌───────▼────────┐  ┌───────▼────────┐
│Alert System │  │Sensor History  │  │Flash Buffer    │
│- Threshold  │  │- Windowing (60)│  │- Circular NVS  │
│- Hysteresis │  │- Averaging     │  │- Auto-overwrite│
└─────────────┘  └────────────────┘  └────────────────┘
```

### State Machine: Alert System

```
       ┌─────────────┐
       │ ALERT_NONE  │◄──────────────┐
       └──────┬──────┘               │
              │                      │
         threshold                   │
         exceeded                    │
              │                 clear_alert()
       ┌──────▼──────────┐           │
       │ ALERT_WARNING   │───────────┤
       │ (LED Yellow)    │           │
       └──────┬──────────┘           │
              │                      │
       critical count                │
       threshold                     │
              │                      │
       ┌──────▼──────────┐           │
       │ ALERT_CRITICAL  │───────────┘
       │ (LED Red+Buzzer)│
       └─────────────────┘
              │
              │ snooze_active
              ▼
       ┌─────────────────┐
       │ SNOOZE (5 min)  │
       │ (Buzzer muted)  │
       └─────────────────┘
```

**Hysteresis Logic:** Estado não pode retroceder (WARNING → NONE) sem chamada explícita a `alerts_clear_alert()`. Previne oscilação em valores próximos aos limites.

## Estruturas de Dados e Compactação

### Data Compression Strategy

**RAM Representation (16 bytes):**
```c
typedef struct {
    float temperature;   // 4 bytes, IEEE 754
    float humidity;      // 4 bytes
    float lux;          // 4 bytes
    float noise_level;   // 4 bytes (RMS percentage 0-100)
} full_sensor_read_t;
```

**Flash Representation (5 bytes):**
```c
typedef struct __attribute__((packed)) {
    uint8_t temperature : 6;   // Range: 0-63 → 0-50°C (step 0.79°C)
    uint8_t humidity : 7;      // Range: 0-127 → 0-100% (step 0.79%)
    uint16_t lux : 15;         // Range: 0-32767 → 0-16384 lux (step 0.5)
    uint16_t noise_level : 12; // Range: 0-4095 → 0-100% (step 0.024%)
} compact_sensor_read_t;  // 40 bits = 5 bytes
```

**Compression Ratio:** 16:5 = 3.2:1

**Encoding Macros:**
```c
#define COMPACT_TEMP(t)  (uint8_t)((t) * 1.26)           // 0-50°C → 0-63
#define COMPACT_HUM(h)   (uint8_t)((h) * 1.27)           // 0-100% → 0-127
#define EXPAND_NOISE(n)  ((float)(n) / 40.95)
```

### Flash Record Structure

**Persistent Record (9 bytes):**
```c
typedef struct __attribute__((packed)) {
    uint32_t timestamp;               // Unix epoch (4 bytes)
    compact_sensor_read_t compact;    // Compressed data (5 bytes)
} flash_record_t;
```

**Storage Capacity Calculation:**
- NVS partition: ~24KB disponível
- Buffer: 1440 records × 9 bytes = 12.96 KB
- Overhead: ~20% (metadata NVS)
- Total: ~15.5 KB

**Circular Buffer Implementation:**
```c
typedef struct {
    nvs_handle_t nvs_handle;
    char namespace[16];
    uint32_t write_index;      // Próxima posição de escrita
    uint32_t sample_count;     // Total de amostras (até max_samples)
    uint32_t max_samples;      // Capacidade máxima (1440)
    size_t sample_size;        // sizeof(flash_record_t)
} flash_buffer_t;
```

**Auto-Overwrite Logic:**
```c
if (buffer->sample_count >= buffer->max_samples) {
    // Apaga amostra mais antiga (na posição write_index)
    nvs_erase_key(buffer->nvs_handle, old_key);
}
// Escreve nova amostra na mesma posição
nvs_set_blob(buffer->nvs_handle, key, sample, size);
buffer->write_index = (buffer->write_index + 1) % buffer->max_samples;
```

### Memory Layout

**Total SRAM Usage (approximate):**
```
Flash Buffer struct:      80 bytes (heap)
Sensor History (60):     960 bytes (heap) = 60 × 16 bytes
Current Read:             16 bytes (BSS)
Alert State:              20 bytes (BSS)
MAX9814 Sample Buffer:  4096 bytes (heap) = 1024 × 4 bytes
Task Stacks:           20480 bytes = 5 tasks × 4096 bytes
Total Runtime:         ~25 KB
```

**Flash Usage:**
```
Code (.text):        ~200 KB
Read-only data:       ~20 KB
NVS Data:             ~16 KB (sensores) + 8 KB (sistema)
Total:               ~244 KB (deixa ~3.8 MB livres em 4MB flash)
```

## Hardware Interfaces

### Peripheral Mapping

**Digital I/O:**
- GPIO 23: DHT11 (1-Wire protocol, bit-banged, pull-up 10kΩ)
- GPIO 22: Button input (internal pull-up, interrupt on both edges)
- GPIO 32: LED Yellow (output, active high)
- GPIO 26: LED Red (output, active high)

**I2C Bus (Master mode):**
- ESP32-WROOM: SDA=GPIO 18, SCL=GPIO 19
- ESP32-S2: SDA=GPIO 21, SCL=GPIO 20
- Clock: 100 kHz (standard mode)
- BH1750FVI address: 0x23 (ADDR pin → GND)

**ADC (12-bit resolution):**
- GPIO 33: ADC1_CH5 (MAX9814 microphone output)
- Attenuation: 11dB → 0-3.3V range → ADC values 0-4095
- Mode: Oneshot (polling) with 125µs inter-sample delay

**PWM (LEDC - LED Control):**
- GPIO 15: Buzzer output
- Timer: LEDC_TIMER_0, Channel: LEDC_CHANNEL_0
- Frequency: 800 Hz, Resolution: 12-bit (duty cycle 0-4095)
- Mode: Low-speed mode (APB_CLK source)

**UART0 (Console/Debug):**
- TX: GPIO 1, RX: GPIO 3
- Baud rate: 115200, 8N1
- Used for: ESP-IDF logging, JSON data dump

### Power Management

**Active Mode:**
- CPU: 240 MHz (dual-core)
- WiFi: OFF (exceto durante sync NTP)
- Peripherals: ADC, I2C, GPIO, LEDC ativos
- Estimated: ~80mA @ 3.3V

**WiFi Active (NTP sync):**
- Duration: ~5-10 segundos na inicialização
- Peak current: ~160mA @ 3.3V
- Desligado após sincronização bem-sucedida

## Módulos de Software

### Core System (`src/`)

**sensor_monitor.c** - Sensor Orchestration
- Abstrai criação de tasks periódicas para qualquer sensor
- Software Timer + Task Notification para scheduling
- Timeout handling e error recovery
- Generic callback dispatch

**sensor_history.c** - Windowed Averaging
- Circular buffer de 60 amostras em RAM
- Cálculo de média móvel com acumulador
- Compactação float → bitfield para persistência
- Interface para flush automático para flash

**alerts.c** - State Machine Implementation
- Finite State Machine: NONE → WARNING → CRITICAL
- Mutex-protected state transitions (thread-safe)
- LED control via GPIO, buzzer via LEDC PWM
- Snooze mechanism com timer temporário

### Sensor Drivers (`src/`)

**dht11_sensor.c** - 1-Wire Protocol
- Bit-banging implementation (não usa hardware 1-Wire)
- Timing crítico: start condition 18ms LOW pulse
- Checksum validation (último byte = soma dos 4 primeiros)
- Retry logic com exponential backoff

**bh1750fvi_sensor.c** - I2C Light Sensor
- Utiliza biblioteca esp-idf-lib (i2cdev abstraction)
- Modo: Continuous High Resolution (1 lux resolution)
- Measurement time: 120ms típico
- I2C error handling com retry

**noise_sensor.c** - ADC Wrapper
- Interface para biblioteca MAX9814
- Conversão RMS → percentual para alertas
- Configuração: 1024 samples @ 8kHz = 128ms acquisition time

### Libraries (`lib/`)

**flash_buffer/** - NVS Circular Buffer
- Generic implementation: qualquer tipo de dado
- Key generation: `s_<index>` para cada amostra
- Metadata persistence: `w_idx`, `count`
- Auto-overwrite quando buffer cheio (desde dezembro 2025)

**max9814/** - DSP Audio Processing
- ADC oneshot mode com delays precisos (125µs)
- RMS calculation: √(Σ(x - mean)² / N)
- DC bias removal antes do RMS
- taskYIELD() a cada 128 samples (16ms) previne watchdog

### Dependency Tree

```
main.c
├── sensor_monitor.h
│   ├── sensor_base.h
│   └── FreeRTOS (tasks, timers)
├── sensor_history.h
│   └── flash_buffer.h (lib)
│       └── nvs_flash.h (ESP-IDF)
├── alerts.h
│   ├── driver/gpio.h
│   └── driver/ledc.h
├── dht11_sensor.h
│   └── dht.h (components/)
├── bh1750fvi_sensor.h
│   └── i2cdev.h (managed_components/)
├── noise_sensor.h
│   └── max9814.h (lib)
│       └── esp_adc/adc_oneshot.h
└── time_sync.h
    ├── esp_wifi.h
    └── esp_sntp.h
```

## Estruturas de Dados (Legado)

**Nota:** Seção mantida para referência de estruturas duplicadas encontradas no código.

### `compact_sensor_read_t` - Compactação
### `compact_sensor_read_t` - Compactação

```c
typedef struct __attribute__((packed)) {
    uint8_t temperature : 6;   // 0-63 → 0-50°C (step 0.79°C)
    uint8_t humidity : 7;      // 0-127 → 0-100% (step 0.79%)
    uint16_t lux : 15;         // 0-32767 → 0-16384 lux (step 0.5)
    uint16_t noise_level : 12; // 0-4095 → 0-100% (step 0.024%)
} compact_sensor_read_t;  // Total: 40 bits = 5 bytes
```

### `flash_record_t` - Persistência
```c
typedef struct {
    uint32_t timestamp;               // Unix time (segundos)
    compact_sensor_read_t compact;    // Leitura compactada
} flash_record_t;
```

## Configuração de Hardware

### Microcontrolador
- **Modelo**: ESP32-WROOM-32 ou ESP32-S2-Saola-1
- **Framework**: ESP-IDF (Espressif IoT Development Framework)
- **RTOS**: FreeRTOS (integrado ao ESP-IDF)
- **Clock**: 240 MHz (dual-core Xtensa LX6 no ESP32 WROOM)
- **Flash**: 4 MB (mínimo), NVS partition para armazenamento persistente

### Mapeamento de Pinos

#### Sensores
- **GPIO 23**: DHT11 Data (temperatura e umidade)
- **I2C para BH1750FVI** (luminosidade):
  - **ESP32 WROOM**: SDA=GPIO 18, SCL=GPIO 19
  - **ESP32-S2**: SDA=GPIO 21, SCL=GPIO 20
  - Endereço I2C: `0x23` (BH1750_I2C_ADDR_LOW)
- **GPIO 33**: MAX9814 Analog Out (ADC1_CH5 - nível de ruído RMS)
  - Taxa de amostragem: 8kHz (1024 amostras por leitura)
  - Modo ADC: Oneshot com atenuação 11dB (0-3.3V range)

#### Indicadores e Controle
- **GPIO 32**: LED Amarelo (Estado Warning)
- **GPIO 15**: Buzzer PWM (LEDC Channel 0, Timer 0, 800 Hz, 12
- **GPIO 34**: Buzzer PWM (LEDC Channel 0, Timer 0, 2 kHz, 10-bit resolution)
- **GPIO 22**: Botão de Navegação (pull-up interno, any edge interrupt com debouncing)

#### Comunicação
- **GPIO 1/3**: UART0 TX/RX (USB serial, 115200 baud)
- **WiFi**: Interno (para sincronização NTP)

### Alimentação
- **Tensão**: 5V via USB ou 3.3V regulado
- **Consumo**: ~80mA em operação normal, ~160mA com WiFi ativo

## Estrutura do Projeto

```
.
├── platformio.ini           # Configuração PlatformIO (esp32doit-devkit-v1)
├── CMakeLists.txt           # Build system ESP-IDF
├── sdkconfig.*              # Configurações SDK para diferentes boards
├── README.md                # Este arquivo
├── LICENSE                  # Licença do projeto
│
├── main/                    #  Gerenciado pelo IDF Component Manager
│   └── idf_component.yml    # Dependências do componente
│
├── include/                 # Headers públicos
│   ├── sensor_base.h        # Interface base para sensores
│   ├── sensor_monitor.h     # Sistema de monitoramento periódico
│   ├── sensor_history.h     # Histórico em RAM e compactação
│   ├── flash_record.h       # Estrutura de registro na flash
│   ├── dht11_sensor.h       # Driver DHT11
│   ├── bh1750fvi_sensor.h   # Driver BH1750FVI
│   ├── noise_sensor.h       # Wrapper para MAX9814 library
│   ├── alerts.h             # Sistema de alertas (LEDs + buzzer)
│   ├── button_driver.h      # Driver de botão com debouncing
│   ├── time_sync.h          # Sincronização NTP via WiFi
│   └── uart_json_handler.h  # Dump de dados JSON via UART
│
├── src/                     # Implementações
│   ├── sensor_monitor.c
│   ├── sensor_history.c
│   ├── dht11_sensor.c
│   ├── main.c               # Entry point, inicialização, app_main()
│   ├── bh1750fvi_sensor.c
│   ├── noise_sensor.c       # Wrapper para MAX9814
│   ├── alerts.c
│   ├── button_driver.c
│   ├── time_sync.c
│   ├── uart_json_handler.c
│   └── CMakeLists.txt
│
├── lib/                     # Bibliotecas auxiliares
│   ├── flash_buffer/        # Sistema de buffer circular em NVS
│   │   ├── flash_buffer.h
│   │   ├── flash_buffer.c
│   │   └── README.md
│   ├── max9814/             # Driver MAX9814 com ADC oneshot e cálculo RMS
│   │   ├── max9814.h
│   │   ├── max9814.c
│   │   └── README.md
│   └── bh1750/              # Driver I2C BH1750 (legacy)
│
├── components/              # Componentes ESP-IDF externos
│   ├── bh1750/              # Biblioteca esp-idf-lib para BH1750
│   ├── dht/                 # Biblioteca DHT para ESP-IDF
│   └── esp_idf_lib_helpers/ # Helpers da esp-idf-lib
│
├── managed_components/      # Dependências gerenciadas automaticamente
│   ├── esp-idf-lib__i2cdev/
│   └── esp-idf-lib__esp_idf_lib_helpers/
│
├── docs/                    # Documentação
│   ├── Projeto De Monitoramento Ambiental Para Biotério – Sistemas Embarcados.pdf
│   ├── BH1750FVI - Sensor ICs.pdf
│   ├── DHT11_Datasheet.pdf
│   └── KY-037-datasheet.pdf
│
└── test/                    # Testes unitários (futuro)
```

## Algoritmos Críticos

### RMS Calculation (MAX9814 Library)

**Algorithm: AC-coupled RMS com Standard Deviation**
```c
static float calculate_rms(max9814_t *max9814)
{
    if (max9814->sample_index == 0) {
        return 0.0f;
    }

    // Step 1: Calculate mean (DC bias)
    uint64_t sum = 0;
    for (uint32_t i = 0; i < max9814->sample_index; i++) {
        sum += max9814->sample_buffer[i];
    }
    float mean = (float)sum / max9814->sample_index;

    // Step 2: Calculate sum of squared differences from mean
    float sum_squared = 0.0f;
    for (uint32_t i = 0; i < max9814->sample_index; i++) {
        float diff = (float)max9814->sample_buffer[i] - mean;
        sum_squared += diff * diff;
    }

    // Step 3: Calculate RMS (standard deviation)
    float rms = sqrtf(sum_squared / max9814->sample_index);
    return rms;
}
```

**Conversão para Percentual:**
```c
esp_err_t read_max9814(max9814_t *max9814, float *rms_percent)
{
    float rms_raw = calculate_rms(max9814);
    
    // Normalize to percentage (0-100%)
    *rms_percent = (rms_raw / ADC_MAX_VALUE) * 100.0f;
    
    // Clamp to 0-100%
    if (*rms_percent > 100.0f) *rms_percent = 100.0f;
    else if (*rms_percent < 0.0f) *rms_percent = 0.0f;
    
    return ESP_OK;
}
```

**Rationale:**
- DC removal automático: Subtrai média de cada amostra antes do quadrado
- RMS mede apenas variação AC (som), não offset DC
- Percentual facilita threshold comparison independente de ADC resolution
- ADC_MAX_VALUE = 4095 (12-bit ADC)

**Performance:**
- 1024 samples @ 8kHz = 128ms acquisition
- Cálculo: ~2ms (duas passes no buffer)
- Total latency: ~130ms por leitura

### Moving Average (Sensor History)

**Algorithm: Naive Summation com Validação**
```c
uint32_t get_moving_average(compact_sensor_read_t *avg) {
    if (!history_buffer || max_history_records == 0) {
        return 0;
    }

    int64_t sum_temp = 0;
    uint64_t sum_humid = 0;
    uint64_t sum_lux = 0;
    uint64_t sum_noise = 0;
    size_t count = 0;

    // Soma todos os registros válidos
    for (size_t i = 0; i < max_history_records; i++) {
        compact_sensor_read_t *record = &history_buffer[i];
        // Considera apenas registros válidos (não zero)
        if (record->temperature != 0 || record->humidity != 0 ||
            record->lux != 0 || record->noise_level != 0) {
            sum_temp += record->temperature;
            sum_humid += record->humidity;
            sum_lux += record->lux;
            sum_noise += record->noise_level;
            count++;
        }
    }

    // Calcula média
    if (count > 0) {
        avg->temperature = sum_temp / count;
        avg->humidity = sum_humid / count;
        avg->lux = sum_lux / count;
        avg->noise_level = sum_noise / count;
        return count;
    }
    return 0;
}
```

**Estrutura do Buffer:**
```c
static compact_sensor_read_t *history_buffer = NULL;
static size_t current_record_index = 0;
static size_t max_history_records = 0;

void save_sensor_read(const full_sensor_read_t *read) {
    // Compacta e salva no índice atual
    full_to_compact(read, &history_buffer[current_record_index]);
    
    // Atualiza o índice circularmente
    current_record_index = (current_record_index + 1) % max_history_records;
}
```

**Complexity:** O(N) - Percorre todos os 60 registros a cada cálculo de média

### Alert Threshold Detection

**Algorithm: Direct Comparison com Hysteresis**
```c
esp_err_t check_safe_clean_alerts()
{
    // Verifica se há alerta ativo
    if (alert_status == ALERT_NONE)
        return ESP_ERR_NOT_ALLOWED;

    // Verifica limites do biotério
    if (night_mode == 1 && current_read.lux > LUX_NIGHT_MAX)
        return ESP_ERR_INVALID_STATE;
    else if (current_read.lux > LUX_DAY_MAX || current_read.lux < LUX_DAY_MIN)
        return ESP_ERR_INVALID_STATE;
    else if (current_read.temperature > TEMP_MAX || current_read.temperature < TEMP_MIN)
        return ESP_ERR_INVALID_STATE;
    else if (current_read.humidity > HUM_MAX || current_read.humidity < HUM_MIN)
        return ESP_ERR_INVALID_STATE;
    else if (current_read.noise_level >= NOISE_AVG_LIMIT)
        return ESP_ERR_INVALID_STATE;

    return ESP_OK;  // Todos os parâmetros dentro dos limites
}
```

**Implementação em `app_main()`:**
```c
uint8_t hysteresis = 0;
for (;;) {
    if (check_safe_clean_alerts() == ESP_OK)
        hysteresis++;
    else
        hysteresis = 0;

    if (hysteresis >= ALERT_RESET_HYSTERESIS)
        alerts_clear_alert();
    
    vTaskDelay(pdMS_TO_TICKS(10000));  // Check a cada 10s
}
```

**Anti-flapping:** Contador `hysteresis` acumula leituras dentro dos limites. Apenas limpa alerta após `ALERT_RESET_HYSTERESIS` verificações consecutivas OK.

### Button Debouncing

**Algorithm: ISR + Task com Software Debounce**
```c
static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    Button_t *btn = (Button_t *)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    // Notifica a task do botão
    vTaskNotifyGiveFromISR(btn->task_to_notify, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void button_task(void *pvParameters)
{
    Button_t *btn = (Button_t *)pvParameters;
    bool long_press_handled = false;
    
    while (1) {
        // Calcula wait time para long press detection
        TickType_t wait_time = portMAX_DELAY;
        if (btn->is_pressed && !long_press_handled) {
            int64_t elapsed = (esp_timer_get_time() / 1000) - btn->press_start_time;
            wait_time = (elapsed < LONG_PRESS_MS) ? 
                        pdMS_TO_TICKS(LONG_PRESS_MS - elapsed) : 0;
        }
        
        // Aguarda notificação da ISR ou timeout
        uint32_t notified = ulTaskNotifyTake(pdTRUE, wait_time);
        
        // Debounce: aguarda 50ms após notificação
        if (notified > 0) {
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS));
        }
        
        int level = gpio_get_level(btn->pin);
        int64_t now = esp_timer_get_time() / 1000;
        
        // Detecta pressionamento
        if (level == 0 && !btn->is_pressed) {
            btn->is_pressed = true;
            btn->press_start_time = now;
            long_press_handled = false;
        }
        // Detecta long press (ainda pressionado)
        else if (btn->is_pressed && level == 0 && 
                 !long_press_handled && 
                 (now - btn->press_start_time >= LONG_PRESS_MS)) {
            long_press_handled = true;
            if (btn->callback)
                btn->callback(btn->pin, BUTTON_PRESS_LONG);
        }
        // Detecta release
        else if (btn->is_pressed && level == 1) {
            btn->is_pressed = false;
            if (!long_press_handled) {
                int64_t duration = now - btn->press_start_time;
                button_event_t event = (duration >= NORMAL_PRESS_MS) ?
                                       BUTTON_PRESS_NORMAL : BUTTON_PRESS_SHORT;
                if (btn->callback)
                    btn->callback(btn->pin, event);
            }
        }
    }
}
```

**Parâmetros:**
- `DEBOUNCE_TIME_MS`: 50ms (software delay após ISR)
- `LONG_PRESS_MS`: 2000ms (2 segundos)
- `NORMAL_PRESS_MS`: 500ms (meia segundo)

**Flow:** ISR → Task notification → Debounce delay → GPIO read → Event classification

## Challenges e Soluções Implementadas

### 1. Timer Service Stack Overflow (RESOLVIDO)

**Problema:** 
```
***ERROR*** A stack overflow in task Tmr Svc has been detected.
```

**Root Cause Analysis:**
- FreeRTOS Timer Service task executa callbacks de software timers
- Cada sensor monitor registra callback que pode:
  - Alocar temporários em stack
  - Chamar funções de alerta (chain complexa)
  - Processar dados do sensor
- Stack original: 2048 bytes insuficiente para depth da call stack

**Solução:**
```c
// sdkconfig.upesy_wroom
CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH=4096  // era 2048
```

**Justificativa:** Profiling com `uxTaskGetStackHighWaterMark()` mostrou <200 bytes livres. Dobrar stack eliminou overflow com margem segura (~1KB livre).

### 2. Noise Sensor Data Not Reported (BUG CRÍTICO)

**Problema:**
- MAX9814 calculava RMS corretamente (logs confirmavam)
- Valor nunca aparecia no sistema de alertas ou histórico
- Comportamento undefined: ponteiro não inicializado sendo lido

**Root Cause:**
```c
// noise_sensor.c (ANTES - BUGADO)
esp_err_t noise_sensor_read_data(sensor_base_t *sensor, void *data) {
    float rms_percent = read_max9814(max9814);
    ESP_LOGI(TAG, "RMS: %.2f%%", rms_percent);
    // FALTAVA: *(float *)data = rms_percent;
    return ESP_OK;
}
```

**Impacto:**
- Callback recebia ponteiro `data` mas função nunca escrevia nele
- Stack corruption potencial ao ler valor não inicializado
- Contribuiu para instabilidade geral do sistema

**Fix:**
```c
// noise_sensor.c (DEPOIS - CORRETO)
esp_err_t noise_sensor_read_data(sensor_base_t *sensor, void *data) {
    float rms_percent = read_max9814(max9814);
    *(float *)data = rms_percent;  // ← CRÍTICO
    ESP_LOGI(TAG, "RMS: %.2f%%", rms_percent);
    return ESP_OK;
}
```

### 3. ADC DMA Implementation (Abandoned)

**Tentativa Inicial:**
```c
// Implementação DMA (ABANDONADA)
adc_continuous_handle_t adc_handle;
adc_continuous_config_t config = {
    .pattern_num = 1,
    .sample_freq_hz = 8000,
    .conv_mode = ADC_CONV_SINGLE_UNIT_1,
};
adc_continuous_new_unit(&config, &adc_handle);
adc_continuous_start(adc_handle);
```

**Problema:**
```
rst:0x8 (TG1WDT_SYS_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
Brownout detector was triggered
```

**Root Cause:**
- DMA blocking em `adc_continuous_read()` com timeout insuficiente
- Hardware ADC não disponibilizava amostras na taxa esperada
- Watchdog timer expirando durante espera bloqueante
- System brownout falso (ruído na linha de alimentação)

**Solução Final: Oneshot Mode**
```c
// max9814.c - Implementação atual
adc_oneshot_unit_handle_t adc_handle;
for (int i = 0; i < num_samples; i++) {
    adc_oneshot_read(adc_handle, channel, &raw);
    buffer[i] = raw;
    esp_rom_delay_us(delay_us);  // 125µs = 8kHz
    
    if (i % 128 == 0) {
        taskYIELD();  // Previne watchdog
    }
}
```

**Trade-offs:**
- ✅ Simplicidade: Sem configuração complexa de DMA
- ✅ Confiabilidade: Sem watchdog resets
- ✅ Controle fino: delay_us ajustável
- ❌ CPU overhead: Loop ativo vs DMA assíncrono
- ❌ Timing jitter: ~±10µs por amostra (aceitável para 8kHz)

**Performance Impact:** CPU usage ~15% durante aquisição (128ms a cada 1s). Aceitável dado estabilidade obtida.

### 4. Flash Buffer Full Error

**Problema Inicial:**
```
E (9561658) flash_buffer: Erro ao salvar amostra: ESP_ERR_NVS_NOT_ENOUGH_SPACE
```

**Root Cause:**
- Buffer circular não implementava overwrite automático
- `nvs_set_blob()` falhava quando NVS partition cheia
- Sistema continuava operando mas sem persistência

**Solução: Auto-Overwrite**
```c
esp_err_t flash_buffer_write(flash_buffer_t *buffer, const void *sample) {
    // Se cheio, apaga entrada mais antiga antes de escrever
    if (buffer->sample_count >= buffer->max_samples) {
        char old_key[16];
        snprintf(old_key, sizeof(old_key), "s_%lx", buffer->write_index);
        nvs_erase_key(buffer->nvs_handle, old_key);
    }
    
    // Escreve nova entrada na posição liberada
    nvs_set_blob(buffer->nvs_handle, key, sample, size);
    buffer->write_index = (buffer->write_index + 1) % buffer->max_samples;
}
```

**Benefit:** True circular buffer behavior - últimas 1440 amostras sempre disponíveis.

### 5. Thread Safety em Alert Status

**Problema Potencial:**
- `alert_status` global acessado por múltiplas tasks
- Sensor callbacks escrevem, task_alert lê
- Race condition teórica (não manifestada em testes)

**Solução Preventiva:**
```c
static SemaphoreHandle_t alert_mutex = NULL;

// Write (sensor callbacks)
if (xSemaphoreTake(alert_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
    alert_status = new_status;
    xSemaphoreGive(alert_mutex);
    xTaskNotifyGive(alert_task_handle);
}

// Read (task_alert)
if (xSemaphoreTake(alert_mutex, portMAX_DELAY) == pdTRUE) {
    alert_t local_status = alert_status;
    xSemaphoreGive(alert_mutex);
    // Processa local_status sem segurar mutex
}
```

**Pattern:** Copy-on-read minimiza tempo de lock do mutex.

### 6. DHT11 Timing Sensitivity

**Challenge:** Protocolo 1-Wire do DHT11 requer timings precisos (±5µs tolerance)

**Problem:** FreeRTOS task switches podem causar jitter

**Solution:**
```c
// dht.c - Critical section
portDISABLE_INTERRUPTS();
// Bit-banging sequence com delays calibrados
gpio_set_level(pin, 0);
esp_rom_delay_us(18000);  // Start signal
gpio_set_level(pin, 1);
// ... leitura de bits
portENABLE_INTERRUPTS();
```

**Trade-off:** Desabilitar interrupções por ~5ms aceitável dado baixa frequência de leituras (2s interval).

## Configuração do Sistema

### Build Configuration

**PlatformIO Environment:**
```ini
[env:upesy_wroom]
platform = espressif32
board = esp32doit-devkit-v1
framework = espidf
monitor_speed = 115200
board_build.flash_size = 4MB
```

**ESP-IDF SDKConfig (Critical Settings):**
```
CONFIG_FREERTOS_TIMER_TASK_STACK_DEPTH=4096
CONFIG_FREERTOS_HZ=1000
CONFIG_ESP_TASK_WDT_TIMEOUT_S=5
CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=4096
```

### Parâmetros de Tempo dos Sensores

| Sensor | Intervalo | Amostras/Hora | Justificativa |
|--------|----------|--------------|--------------|
| DHT11  | 2000ms   | 1800         | Min de 1.5s conforme datasheet, 2s para margem |
| BH1750 | 10000ms  | 360          | Luz varia lentamente, economiza I2C |
| MAX9814| 1000ms   | 3600         | RMS precisa de atualizações frequentes |

**Alert Thresholds:**
```c
#define TEMP_WARNING_COUNT  30   // 5min @ 10s interval
#define TEMP_CRITICAL_COUNT 60   // 10min
#define LIGHT_WARNING_COUNT 12   // 2min @ 10s interval
#define LIGHT_CRITICAL_COUNT 30  // 5min
#define NOISE_WARNING_TIME  40   // 40 seconds accumulated
```

## Metricas de performace

**Memory Footprint:**
- Code: ~200 KB (.text section)
- Data: ~25 KB (heap + stacks)
- NVS: ~16 KB (sensor data) + 8 KB (system)

**Uso de CPU (médio):**
- Idle: ~5% (sensor monitoring overhead)
- ADC acquisition: +15% burst durante 128ms a cada 1s
- WiFi/NTP: +40% durante 5-10s na inicialização

**Ciclos de Escrita em Flash:**
- 1 escrita/minuto (média móvel salva)
- ~1440 escritas/dia
- NVS especificada para 100K ciclos → ~69 dias de operação contínua por célula
- Wear leveling da NVS estende para anos de operação

**Latência:**
- Sensor → Callback: <10ms (jitter de scheduling)
- Detecção de alerta → LED ligado: <50ms
- Pressão de botão → Início do dump de dados: <100ms

## Considerações de Segurança

**Implementação Atual:**
- Credenciais WiFi em texto plano no código (senha WiFi hardcoded)
- NVS não criptografada
- UART sem autenticação

**Recomendações para Produção:**
- Usar NVS encryption (ESP32 flash encryption feature)
- Armazenar credenciais WiFi em NVS com chave separada
- Implementar challenge-response para dump de dados via UART
- Adicionar TLS para comunicações de rede (se MQTT implementado)

## Estratégia de Testes

**Testes Unitários (não implementados ainda):**
- [ ] RMS calculation com sinais conhecidos
- [ ] Compactação/descompactação (reversibilidade)
- [ ] Flash buffer circular overflow
- [ ] Alert state machine transitions

**Integration Tests:**
- [x] Operação contínua por 24h (teste de estresse)
- [x] Cenário de buffer flash cheio
- [x] Tratamento de falha de conexão WiFi
- [x] Recuperação de desconexão de sensor

**Validação de Hardware:**
- [x] DHT11 vs termômetro calibrado (±1°C accuracy confirmed)
- [x] BH1750 vs luxímetro comercial (±10% accuracy)
- [x] MAX9814 RMS vs osciloscópio (qualitative match)

## Referências Técnicas

**Datasheets:**
- ESP32-WROOM-32 Datasheet (Espressif)
- DHT11 Digital Temperature-Humidity Sensor
- BH1750FVI Digital 16-bit Serial Output Ambient Light Sensor IC
- MAX9814 Microphone Amplifier with AGC and Low-Noise Microphone Bias

**Bibliotecas:**
- ESP-IDF v5.x API Reference
- FreeRTOS Kernel Documentation v10.x
- esp-idf-lib (I2C device drivers)

---

**Desenvolvido como projeto acadêmico de Sistemas Embarcados**  
**Implementação: Dezembro 2025**