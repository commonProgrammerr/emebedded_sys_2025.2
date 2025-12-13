# Sistema de Monitoramento Ambiental para Biotério (MVP)

**Solução embarcada completa e integrada para monitoramento contínuo de condições ambientais em salas de criação de ratos**, com foco em parâmetros críticos para reprodução. O sistema monitora temperatura, umidade relativa, iluminação e ruído, com alertas em tempo real e histórico de dados persistente em flash.

---

## Visão Geral das Funcionalidades

### Sensores Integrados

| Sensor | Parâmetro | Faixa-Alvo | Amostragem | Alerta |
|--------|-----------|-----------|-----------|--------|
| **DHT11/DHT22** | Temperatura | 22–26 °C | 2 s (média 1 min) | ≥ 3 leituras fora |
| **DHT11/DHT22** | Umidade | 40–60 % | 2 s (média 1 min) | ≥ 3 leituras fora |
| **BH1750FVI** | Iluminância (dia) | 150–300 lux | 10 s | > 5 min fora |
| **BH1750FVI** | Luz noturna (vazamento) | ~0 lux (tol: ≤ 5 lux) | 10 s | > 2 min acima |
| **KY-037 (LM393)** | Ruído (score 0–100) | Média < 40–50 | 1 s (RMS 1s) | Picos > 60 por > 3 s |

### Atuadores de Alerta

- **LED Amarelo (GPIO 32)**: Aviso – 1–5 min fora dos limites
- **LED Vermelho (GPIO 26)**: Alarme – > 5 min fora ou múltiplas variáveis críticas
- **Buzzer PWM (GPIO 15)**: Pulsos periódicos (atenção) ou contínuo (alarme); opção de snooze por 5 min
- **Display OLED**: Exibição em tempo real de valores, status e tendências

### Processamento de Dados

- **Armazenamento em Flash**: Buffer circular em NVS (24–48 h de histórico)
- **Conformidade**: Cálculo automático de % de tempo dentro das faixas (meta: ≥ 85%)
- **Média Móvel**: Suavização de 1 min com histerese de 3 leituras
- **Interfaceamento UART/JSON**: Exportação de dados, configuração remota e sincronização de horário
- **Máquina de Estados**: Transições automáticas: OK → Atenção → Alarme → OK

---

## Arquitetura do Sistema

### Pilha Tecnológica

- **Microcontrolador**: ESP32 / ESP32-S2 (32-bit dual-core, 240 MHz)
- **RTOS**: FreeRTOS com Tasks, Software Timers e Queues
- **Persistência**: NVS (Non-Volatile Storage) em flash
- **Comunicação**: 
  - I2C para BH1750 (sensor de luz)
  - GPIO/Interrupts para DHT11 (temperatura/umidade)
  - ADC para KY-037 (ruído)
  - UART para JSON (debug e exportação)
- **Periféricos**: LEDs (GPIO), Buzzer (PWM/LEDC), Botão (EXTI)

### Diagrama Simplificado

```
┌────────────────────────────────────────────────────────────────┐
│                      ESP32 Microcontrolador                    │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  ┌──────────────────┐     ┌───────────────────────────────┐    │
│  │  Sensores I/O    │     │  Camada de Processamento      │    │
│  ├──────────────────┤     ├───────────────────────────────┤    │
│  │ • DHT11 (GPIO23) │     │ • FreeRTOS Tasks              │    │
│  │ • BH1750 (I2C)   │     │ • Software Timers             │    │
│  │ • KY-037 (ADC5)  │     │ • State Machine (4 estados)   │    │
│  │ • Button (GPIO22)│     │ • Média Móvel & Histerese     │    │
│  └──────────────────┘     │ • Buffer Circular (Flash)     │    │
│                           └───────────────────────────────┘    │
│  ┌──────────────────┐     ┌───────────────────────────────┐    │
│  │  Atuadores       │     │  Interface & Persistência     │    │
│  ├──────────────────┤     ├───────────────────────────────┤    │
│  │ • LED Vermelho   │     │ • UART / JSON Handler         │    │
│  │ • LED Amarelo    │     │ • NVS Flash Storage           │    │
│  │ • Buzzer (PWM)   │     │ • Sync NTP (se WiFi)          │    │
│  │ • Histerese      │     │ • Exportação CSV/JSON         │    │
│  └──────────────────┘     └───────────────────────────────┘    │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

### Máquina de Estados (4 Estados)

```
┌──────────────┐
│ STATE_INIT   │  Inicialização e testes de sensores
│ (PowerOn)    │  • Testa I2C com BH1750
│              │  • Valida DHT11 e KY-037
└─────┬────────┘  • Sincroniza relógio (NTP)
      │
      ├──(sucesso)──→ STATE_NORMAL ◄──┐
      │               (operação)      │
      │               • Leitura contínua
      │               • Dentro das faixas
      │               • LED desligado/verde
      │                 │
      │                 │ (fora de limite)
      │                 ↓
      │              STATE_ALERT
      │              (alerta)
      │              • 1+ variáveis fora
      │              • 1–5 min: LED amarelo
      │              • > 5 min: LED vermelho
      │              • Buzzer ativo
      │                 │
      │                 └─(volta ao normal)→┘
      │
      └──(erro)──→ STATE_ERROR
                   (falha)
                   • Sensor não responde
                   • LED vermelho piscante
                   • Buzzer contínuo
                   • Tenta recuperação automática
```

### Fluxo Operacional Detalhado

#### **Fase de Inicialização (STATE_INIT → STATE_NORMAL)**
1. FreeRTOS scheduler inicia
2. I2C configurado, BH1750 testado
3. DHT11 e KY-037 inicializados
4. NVS flash verificado e formatado se necessário
5. NTP sync iniciado (se conectado a WiFi)
6. Tasks e Software Timers criadas
7. Transição para STATE_NORMAL

#### **Operação Normal (STATE_NORMAL)**
1. **Timer periodicidade**:
   - DHT11: a cada 2 s
   - BH1750: a cada 10 s
   - KY-037: a cada 1 s
2. **Processamento**:
   - Armazena amostra em buffer circular
   - Calcula média móvel de 1 min
   - Compara com limites
3. **Decisão**:
   - Se dentro → LED desligado, nenhum alerta
   - Se fora por ≥ 3 leituras → STATE_ALERT
4. **Persistência**:
   - A cada 5 min: escreve agregados em NVS

#### **Alerta (STATE_ALERT)**
1. **Escalada temporal**:
   - 1–5 min fora: LED amarelo, buzzer pulsos (30 em 30 s)
   - > 5 min: LED vermelho, buzzer contínuo
   - > múltiplas variáveis críticas: LED vermelho, buzzer contínuo
2. **Ações do usuário**:
   - Botão clique: ativa snooze (buzzer silencia por 5 min)
   - Botão longo: force reset do alerta
3. **Volta a STATE_NORMAL**:
   - Quando 3 leituras consecutivas voltam aos limites (histerese)

#### **Erro (STATE_ERROR)**
1. **Detecção**:
   - Falha de I2C com BH1750 (não responde)
   - DHT11 com taxa de erro > 50%
   - Flash corrompido
2. **Resposta**:
   - LED vermelho piscante (100 ms on/off)
   - Buzzer contínuo até intervenção
3. **Recuperação**:
   - Tenta reinicializar sensores a cada 10 s
   - Retorna a STATE_NORMAL se sucesso
   - Mantém-se em STATE_ERROR se falha persistir

---

## Módulos de Software

### 1. **Drivers de Sensores** (`src/` + `include/`)

#### `dht11_sensor.h / dht11_sensor.c`
- Protocolo 1-Wire com DHT11/DHT22
- Retorna `dht11_data_t { temperature, humidity }`
- Intervalo: 2 s (configurável em `env.h`)
- Implementa `sensor_base_t` interface

#### `bh1750fvi_sensor.h / bh1750fvi_sensor.c`
- I2C com modo de sensibilidade automática
- Retorna lux (0–54000 lux)
- Suporta ajuste de sensibilidade (High, Medium, Low)
- Intervalo: 10 s (configurável)

#### `KY-037_sensor.h / KY-037_sensor.c`
- ADC do microfone LM393
- Cálculo RMS em janela de 1 s
- Score 0–100 (linear do ADC 0–4095)
- Intervalo: 1 s (configurável)

### 2. **Monitoramento Periódico** (`sensor_monitor.h / sensor_monitor.c`)
- Wrapper que encapsula leitura assíncrona
- FreeRTOS Software Timers + Tasks
- Callbacks após cada leitura bem-sucedida
- Máximo 3 sensores simultâneos

**Exemplo de uso:**
```c
sensor_monitor_t* monitor = new_sensor_monitor(
    &dht11_sensor,          // Sensor base
    2000,                   // Intervalo 2 s (em ms)
    sizeof(dht11_data_t),   // Tamanho dos dados
    "DHT11",                // Nome
    on_dht11_read           // Callback
);
start_sensor_monitoring(monitor);
```

### 3. **Histórico e Conformidade** (`sensor_history.h / sensor_history.c`)
- Estrutura `full_sensor_read_t { temperature, humidity, lux, noise_level }`
- Buffer circular com média móvel
- Função `compute_compliance_percentage()`:
  - Calcula % de amostras dentro das faixas em 24 h
  - Meta: ≥ 85%
- Função `get_min_max_avg()` para estatísticas

### 4. **Sistema de Alertas** (`alerts.h / alerts.c`)
- Estados: `ALERT_NONE`, `ALERT_WARNING`, `ALERT_CRITICAL`
- Controla LEDs (GPIO) e Buzzer (PWM/LEDC)
- Queue assíncrona de alertas
- Função `alerts_snooze(duration_ms)` para silenciar temporariamente

**Exemplo:**
```c
alerts_send_alert(ALERT_WARNING, "Temperature out of range!");
alerts_snooze(300000);  // Silencia por 5 min
```

### 5. **Máquina de Estados** (`state_machine.h / state_machine.c`)
- Estados: `STATE_INIT`, `STATE_NORMAL`, `STATE_ALERT`, `STATE_ERROR`
- Eventos: `EVENT_INIT_COMPLETE`, `EVENT_SENSOR_OUT_OF_RANGE`, `EVENT_RECOVERY`, etc.
- Callbacks de transição, entrada e saída de estados
- Sincronização com mutex FreeRTOS
- Proteção contra transições inválidas

**Transições válidas:**
```
STATE_INIT → STATE_NORMAL ou STATE_ERROR
STATE_NORMAL ↔ STATE_ALERT (bidirecional)
STATE_NORMAL → STATE_ERROR (falha crítica)
STATE_ERROR → STATE_NORMAL (após recuperação)
Qualquer estado → STATE_INIT (reset)
```

### 6. **Persistência em Flash** (`lib/flash_buffer.h / flash_buffer.c`)
- NVS (Non-Volatile Storage) do ESP-IDF
- Buffer circular com até 48 h de dados (configurável)
- Função `flash_buffer_write()` atômicas
- Função `flash_buffer_read()` com ordenação temporal
- Recovery automático em boot

**Exemplo:**
```c
flash_buffer_t* buffer = flash_buffer_create("sensor_data", 1000);
flash_buffer_write(buffer, &reading);  // Escreve automaticamente na próxima oportunidade
```

### 7. **Interface UART/JSON** (`uart_json_handler.h / uart_json_handler.c`)
- Comunicação serial (115200 baud padrão)
- Comandos estruturados em JSON
- Suporta: `get_status`, `export_data`, `set_limits`, `snooze_alert`
- Exemplo de resposta:
```json
{
  "cmd": "get_status",
  "temp": 24.5,
  "humidity": 52.0,
  "lux": 280,
  "noise": 45,
  "state": "NORMAL",
  "compliance": 92.5,
  "timestamp": "2025-12-13T14:30:45Z"
}
```

### 8. **Sincronização de Tempo** (`time_sync.h / time_sync.c`)
- NTP (Network Time Protocol) se WiFi disponível
- Fallback para relógio interno do ESP32
- Timestamps precisos para cada leitura
- Conversão local para horário padrão

### 9. **Driver de Botão** (`button_driver.h / button_driver.c`)
- Debounce de 50 ms
- Detecção de clique (< 500 ms) vs pressionamento longo (≥ 2000 ms)
- Notificações via `xTaskNotify()` para main task
- Estados: single click, long press

---

## Configuração de Hardware

### Pinagem dos Componentes

| Componente | GPIO/Interface | Tipo | Função |
|-----------|----------------|------|--------|
| **DHT11/22** | GPIO 23 | Digital (1-Wire) | Temperature & Humidity |
| **BH1750FVI** | I2C (SDA=21, SCL=19/20) | I2C | Light Sensor (0x23) |
| **KY-037** | ADC Channel 5 (GPIO 33) | Analog ADC | Noise Level |
| **Button (User)** | GPIO 22 | Digital + EXTI | Click / Long Press |
| **LED Red** | GPIO 26 | GPIO Output | Alarm State |
| **LED Yellow** | GPIO 32 | GPIO Output | Warning State |
| **Buzzer** | GPIO 15 (LEDC CH0) | PWM/LEDC | Alert Sound |
| **UART TX** | GPIO 1 | UART | Debug/Export |
| **UART RX** | GPIO 3 | UART | Remote Control |

### Configurações de I2C (BH1750)
- **Endereço**: 0x23 (padrão, não configurable sem modificar hardware)
- **Clock**: 100 kHz (Standard Mode, sem Fast-Mode I2C)
- **Pull-ups**: 4.7 kΩ recomendado (já inclusos na maioria dos breakouts)
- **Barramento**: Dedicado (sem outros dispositivos I2C)

### Configurações ADC (KY-037)
- **Resolução**: 12-bit (0–4095)
- **Atenuação**: 11dB (para range máximo 0–3.6V)
- **Amostragem**: 200 µs (configurável)

### Diagrama de Conexão Simplificado

```
ESP32 DevKit / ESP32-S2 Saola
┌─────────────────────────────────────┐
│                                     │
│ GPIO23──→ DHT11 (Data)              │
│           • GND = GND               │
│           • VCC = 3.3V              │
│                                     │
│ GPIO22──→ Button ── GND             │
│           (Pull-up interno ativo)   │
│                                     │
│ GPIO26──→ LED Red ──[220Ω]── GND    │
│ GPIO32──→ LED Yellow ──[220Ω]── GND │
│                                     │
│ GPIO15──→ Buzzer (PWM)              │
│          • GND = GND                │
│          • VCC = 3.3V/5V (amplifier)│
│                                     │
│ I2C (SDA21, SCL19/20)──→ BH1750     │
│                         ├─ VCC=3.3V │
│                         ├─ GND      │
│                         └─ ADDR=GND │
│                                     │
│ GPIO33 (ADC5)──→ KY-037 (OUT)       │
│                 ├─ GND              │
│                 └─ VCC=3.3V/5V      │
│                                     │
│ GPIO1 (TX) ↔ Serial Monitor         │
│ GPIO3 (RX)  (115200 baud)           │
│                                     │
└─────────────────────────────────────┘
```

---

## Estrutura do Projeto

```
emebedded_sys_2025.2/
├── CMakeLists.txt                 # Build configuration (ESP-IDF)
├── platformio.ini                 # PlatformIO config (alternativo)
├── README.md                      # Este arquivo
├── LICENSE                        # Licença MIT
├── sdkconfig*                     # Configurações ESP-IDF para diferentes placas
│
├── components/                    # Componentes reutilizáveis
│   ├── dht/                      # DHT11/22 (reutilizável)
│   │   ├── CMakeLists.txt
│   │   ├── dht.h
│   │   ├── dht.c
│   │   └── LICENSE
│   │
│   └── esp_idf_lib_helpers/      # Helpers de compatibilidade
│       ├── CMakeLists.txt
│       └── esp_idf_lib_helpers.h
│
├── include/                       # Headers do projeto principal
│   ├── alerts.h                  # Sistema de alertas
│   ├── bh1750fvi_sensor.h        # Driver BH1750
│   ├── button_driver.h           # Driver do botão
│   ├── dht11_sensor.h            # Wrapper DHT11
│   ├── env.h                     # Configurações e limites
│   ├── flash_record.h            # Persistência (legado)
│   ├── KY-037_sensor.h           # Driver ruído
│   ├── sensor_base.h             # Interface base dos sensores
│   ├── sensor_history.h          # Histórico e conformidade
│   ├── sensor_monitor.h          # Monitoramento periódico
│   ├── state_machine.h           # Máquina de estados
│   ├── time_sync.h               # Sincronização NTP
│   └── uart_json_handler.h       # Interface UART/JSON
│
├── lib/                          # Bibliotecas externas
│   ├── bh1750/                  # BH1750 (light sensor lib)
│   │   ├── bh1750.h
│   │   └── bh1750.c
│   │
│   └── flash_buffer/            # Buffer persistente
│       ├── flash_buffer.h
│       ├── flash_buffer.c
│       └── README.md
│
├── src/                         # Código principal
│   ├── CMakeLists.txt
│   ├── main.c                  # Entry point + task main
│   ├── alerts.c                # Sistema de alertas
│   ├── bh1750fvi_sensor.c      # Driver BH1750
│   ├── button_driver.c         # Driver botão
│   ├── dht11_sensor.c          # Wrapper DHT11
│   ├── KY-037_sensor.c         # Driver ruído (ADC)
│   ├── sensor_history.c        # Histórico
│   ├── sensor_monitor.c        # Monitoramento
│   ├── state_machine.c         # Máquina de estados
│   ├── time_sync.c             # Sync NTP
│   └── uart_json_handler.c     # UART/JSON
│
├── main/                       # ESP-IDF main component
│   └── idf_component.yml
│
├── docs/                       # Documentação
│   ├── project_6.md           # Especificação original
│   ├── STATE_MACHINE_USAGE.md # Uso da máquina de estados
│   └── images/                # Diagramas
│       ├── diagrama_de_estados.svg
│       └── ...
│
└── test/                       # Testes (futuro)
    └── README
```

---

## Como Usar

### Pré-requisitos

- **ESP-IDF** (v5.0+) ou **PlatformIO** com suporte a ESP32
- **Git** para clonar o repositório
- **VS Code** com extensões recomendadas (C/C++, CMake, PlatformIO)
- **USB-to-Serial driver** para conexão com ESP32

### 1. Preparação do Ambiente

#### Opção A: ESP-IDF (recomendado)
```bash
# Clone o repositório
git clone <repo_url>
cd emebedded_sys_2025.2

# Configure o ESP-IDF
source $IDF_PATH/export.sh

# Selecion a placa (ESP32, ESP32-S2, etc.)
idf.py set-target esp32

# Compile
idf.py build
```

#### Opção B: PlatformIO
```bash
# Instale a extensão PlatformIO no VS Code
# Abra o projeto no VS Code
# PlatformIO detecta automaticamente e configura o ambiente

# Build
pio run

# Upload (com debug)
pio run --target upload --upload-port COM3
```

### 2. Configuração dos Limites (`include/env.h`)

Ajuste os seguintes defines conforme necessário:

```c
/* Limites de Biotério */
#define TEMP_MIN 22.0
#define TEMP_MAX 26.0

#define HUM_MIN  40.0
#define HUM_MAX  60.0

#define LUX_DAY_MIN 150
#define LUX_DAY_MAX 300
#define LUX_NIGHT_MIN 0
#define LUX_NIGHT_MAX 5  // Tolerância para vazamento

#define NOISE_PEAK_LIMIT 3048
#define NOISE_AVG_LIMIT  2100

/* Tolerâncias de Tempo (em amostras) */
#define DHT_WINDOW_WARNING_TOLERANCE 30   // 5 min × 2 s = 150 s
#define DHT_WINDOW_CRITICAL_TOLERANCE 60  // 10 min

#define LIGHT_WINDOW_WARNING_TOLERANCE 12  // 2 min × 10 s
#define LIGHT_WINDOW_CRITICAL_TOLERANCE 30 // 5 min

/* Intervalos de Amostragem (ms) */
#define DHT11_READ_INTERVAL_MS 2000
#define BH1750_READ_INTERVAL_MS 10000
#define KY037_READ_INTERVAL_MS 1000
```

### 3. Gravação e Conexão

```bash
# Detecte a porta serial
ls /dev/ttyUSB*  # Linux
COM<X>           # Windows

# Compile e grave com ESP-IDF
idf.py -p /dev/ttyUSB0 flash monitor

# Ou com PlatformIO
pio run --target upload --upload-port /dev/ttyUSB0
pio device monitor
```

### 4. Operação Normal

#### Estado Inicial
- Sistema inicia em STATE_INIT
- LEDs e buzzer testados
- Sensores inicializados
- Transição para STATE_NORMAL (LED desligado)

#### Durante Operação
- **Dados visíveis via UART** (115200 baud):
  ```
  [SensorMonitor] DHT11: T=24.5°C, RH=52.0%
  [SensorMonitor] BH1750: 280 lux
  [SensorMonitor] KY-037: 42 (score)
  [StateM] Todos os parâmetros OK
  ```

- **Alertas Visuais/Sonoros**:
  - LED amarelo + buzzer pulsos → temperatura / umidade / luz fora por 1–5 min
  - LED vermelho + buzzer contínuo → alarme crítico (> 5 min ou múltiplas)

- **Interação com Botão**:
  - Clique curto (< 500 ms): Ativa snooze (buzzer silencia por 5 min)
  - Pressionamento longo (≥ 2 s): Reset manual do estado de alerta

#### Exportação de Dados

Via UART, envie comando JSON:
```json
{"cmd": "export_data", "format": "csv", "duration_hours": 24}
```

Resposta (amostra de 24 h):
```csv
timestamp,temperature,humidity,lux,noise_score,state
2025-12-13T00:00:00Z,23.2,48.5,2,35,NORMAL
2025-12-13T01:00:00Z,23.1,49.2,1,32,NORMAL
...
2025-12-13T23:00:00Z,24.8,51.3,280,48,ALERT
Min/Max/Avg: T=22.1/26.3/24.5°C, RH=38/62/50.5%, Lux=0/320/145, Noise=25/78/45
Conformidade: 88.5% (acima do alvo de 85%)
```

### 5. Monitoramento em Tempo Real

**Via Serial Monitor** (VS Code):
- Conecte via porta USB
- Abra a paleta (Ctrl+Shift+P) e procure "Serial Monitor"
- Taxa: 115200 baud

**Via Script Python** (opcional):
```python
import serial
import json

ser = serial.Serial('/dev/ttyUSB0', 115200)

while True:
    if ser.in_waiting > 0:
        line = ser.readline().decode('utf-8').strip()
        if line.startswith('{'):
            data = json.loads(line)
            print(f"T={data['temp']}°C, RH={data['humidity']}%, Lux={data['lux']}")
```

---

## Configuração Avançada

### Ajuste de Histerese

Altere em `include/env.h`:
```c
#define DHT_WINDOW_WARNING_TOLERANCE 30  // Número de amostras antes de alertar
```

- Valor baixo: alerta mais rápido, mais falsos positivos
- Valor alto: alerta mais lento, mais estável

### Modificação de Intervalo de Amostragem

Em `include/env.h`:
```c
#define DHT11_READ_INTERVAL_MS 2000  // 2 s (mín. ~1.5 s para DHT11)
#define BH1750_READ_INTERVAL_MS 10000 // 10 s
#define KY037_READ_INTERVAL_MS 1000   // 1 s (mín. para RMS)
```

### Persistência em Flash

Em `src/main.c`, configure o tamanho do buffer circular:
```c
buffer = flash_buffer_create("sensor_data", 2880);  // 48 h @ 1 min avg
```

### Sincronização NTP

Em `include/time_sync.h`, configure servidor NTP:
```c
#define NTP_SERVER "pool.ntp.org"
#define NTP_TIMEOUT_MS 5000
```

---

## Critérios de Aceite (MVP)

- ☐ **Leituras Estáveis**: T/UR + Lux + Ruído por 24 h sem erros
- ☐ **Display de Status**: Exibe valores atuais + estado (OK/Atenção/Alarme)
- ☐ **Alertas Automáticos**: LED/Buzzer disparam conforme regras (≥ 3 leituras, > 5 min, histerese)
- ☐ **Persistência**: Dados salvos em flash; recuperação automática em reboot
- ☐ **Exportação**: CSV/JSON com mín/máx/média, % conformidade e timestamps
- ☐ **Interação**: Botão permite snooze/reset de alertas
- ☐ **Conformidade**: Cálculo automático: ≥ 85% de tempo dentro das faixas = OK
- ☐ **Documentação**: Diagrama de pinagem, instalação e limites configurados

---

## Troubleshooting

### Problema: BH1750 não responde (I2C error)
**Solução:**
1. Verifique conexões I2C (SDA, SCL, GND, VCC)
2. Confira pull-ups 4.7 kΩ nas linhas I2C
3. Use `i2cdetect -y 1` (Linux) para verificar endereço 0x23
4. Tente alterar velocidade I2C em `env.h`

### Problema: DHT11 leituras falhando (checksum error)
**Solução:**
1. Aumente intervalo em `env.h` (DHT quer ~2 s entre leituras)
2. Adicione capacitor 100 nF entre dados e GND
3. Verifique alimentação (3.3V estável)
4. Se instável, use DHT22 (melhor tolerance)

### Problema: Buzzer não faz som
**Solução:**
1. Verifique GPIO 15 está livre (não conflita com flash)
2. Teste PWM direto: `ledcWrite(0, 128)` em `alerts.c`
3. Se buzzer passivo, verifique polaridade e tensão (3.3V vs 5V)
4. Use amplificador externo se sinal fraco

### Problema: Botão não responde
**Solução:**
1. Teste com `digitalWrite(GPIO_22, HIGH)` (pull-up interno)
2. Verifique debounce de 50 ms em `button_driver.c`
3. Monitore ISR em logs
4. Use multímetro para confirmar press/release

### Problema: Flash corrompida ou cheio
**Solução:**
```bash
# Apague NVS (perderá dados!)
idf.py erase-flash

# Ou parcialmente (ESP-IDF)
idf.py erase-otadata
```

---

##  Referências

### Datasheets de Sensores
- [DHT11/DHT22 Datasheet](https://www.adafruit.com/datasheets/DHT22.pdf)
- [BH1750FVI Datasheet](https://datasheet.lcsc.com/lcsc/2009261808_ROHM-BH1750FVI-TR_C39381.pdf)
- [LM393 Comparator (KY-037)](https://datasheets.maximintegrated.com/en/ds/LM393.pdf)

### Documentação ESP32
- [ESP32 Technical Reference Manual](https://espressif-docs.readthedocs.io/projects/esp32-technical-reference-manual/en/latest/)
- [ESP-IDF Official Docs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [FreeRTOS on ESP32](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html)

### Protocolos de Comunicação
- [I2C Specification](https://www.nxp.com/docs/en/user-manual/UM10204.pdf)
- [1-Wire Protocol (DHT)](https://www.maximintegrated.com/en/design/technical-documents/tutorials/706.html)

### Padrões de Biotério
- [FELASA (Federation European Laboratory Animal Science Association)](https://www.felasa.eu/) – Recomendações de bem-estar animal
- [Guide for the Care and Use of Laboratory Animals (NIH)](https://grants.nih.gov/grants/olaw/guide-for-the-care-and-use-of-laboratory-animals.pdf)

---

## Licença

Este projeto está licenciado sob a **Licença MIT**. Consulte o arquivo [LICENSE](LICENSE) para mais detalhes.

---

## Contribuindo

Para contribuir com melhorias:

1. **Fork** o repositório
2. **Crie um branch** com descrição: `feature/nova-funcionalidade`
3. **Commit** mensagens claras em português
4. **Teste** em hardware (24 h mínimo)
5. **Pull Request** com documentação de mudanças

### Checklist de Contribuição
- ☐ Código compilando sem warnings
- ☐ Logs claros (tags ESP-IDF: `SensorMonitor`, `StateM`, etc.)
- ☐ Testes em múltiplas temperaturas/umidades (se aplicável)
- ☐ README atualizado
- ☐ Sem hardcodes (use `env.h`)

---

## Suporte

Para dúvidas ou problemas:
1. Verifique este README e [docs/](docs/) detalhadamente
2. Consulte [docs/STATE_MACHINE_USAGE.md](docs/STATE_MACHINE_USAGE.md) para máquina de estados
3. Abra uma **Issue** no GitHub com:
   - Placa ESP32 utilizada (DevKit, Saola, etc.)
   - Logs do serial monitor
   - Configuração de limites aplicada
   - Passos para reproduzir problema

---

**Última atualização:** Dezembro 2025  
**Status:** MVP em produção de testes
