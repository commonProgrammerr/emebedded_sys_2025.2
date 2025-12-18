# Sistema de Monitoramento Ambiental para Biotério (MVP)

**Solução embarcada para monitoramento contínuo de condições ambientais em salas de criação de ratos**, com foco em parâmetros críticos para reprodução. O sistema monitora temperatura, umidade relativa, iluminação e ruído, com alertas em tempo real e histórico de dados persistente em flash.

---

## Visão Geral das Funcionalidades

### Sensores Integrados

| Sensor | Parâmetro | Faixa-Alvo | Amostragem | Alerta | Alarme |
|--------|-----------|-----------|-----------|--------|--------|
| **DHT11/DHT22** | Temperatura | 22–26 °C | 2 s | ≥ 5 min | ≥ 10 min |
| **DHT11/DHT22** | Umidade | 40–60 % | 2 s  | ≥ 5 min | ≥ 10 min |
| **BH1750FVI** | Iluminância (dia) | 150–300 lux | 10 s | ≥ 2 min | ≥ 5 min |
| **BH1750FVI** | Luz noturna (vazamento) | ~0 lux (tol: ≤ 5 lux) | 10 s | ≥ 2 min | ≥ 5 min |
| **MAX9814** | Ruído RMS (0–100%) | Média < 40–50% | 1 s (1024 amostras @ 8kHz) | Picos > 60% por > 3 s | N/A |

### Atuadores de Alerta

- **LED Amarelo (GPIO 32)**: Aviso
- **LED Vermelho (GPIO 26)**: Alarme
- **Buzzer PWM (GPIO 15)**: Pulsos periódicos (quando alarme); opção de snooze por 10 min

### Processamento de Dados

- **Armazenamento em Flash**: Buffer circular em NVS (24 h de histórico)
- **Média Móvel**: Suavização de 1 min
- **Interfaceamento UART**: Exportação de dados como JSON
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
  - ADC Oneshot para MAX9814 (ruído RMS)
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
│  │ • DHT11 (GPIO23) │────→│ • FreeRTOS Tasks              │    │
│  │ • BH1750 (I2C)   │────→│ • Software Timers (2s/10s/1s) │    │
│  │ • MAX9814 (ADC5) │────→│ • Sensor Monitor (async)      │    │
│  │ • Button (GPIO22)│────→│ • State Machine (3 estados)   │    │
│  └──────────────────┘     │ • Sensor History (média/comp) │    │
│                           │ • Alerts System (Queue)       │    │
│  ┌──────────────────┐     └───────────────────────────────┘    │
│  │  Atuadores       │              ↕ Callbacks                 │
│  ├──────────────────┤     ┌───────────────────────────────┐    │
│  │ • LED Vermelho   │←────│  Interface & Persistência     │    │
│  │ • LED Amarelo    │←────├───────────────────────────────┤    │
│  │ • Buzzer (PWM)   │←────│ • Exportação JSON             │    │
│  │ • OLED Display   │←────│ • Flash Buffer (NVS circular) │    │
│  └──────────────────┘     │ • Time Sync (NTP fallback)    │    │
│                           └───────────────────────────────┘    │
│                                                                │
│  Fluxo: Sensores → Monitor → History → StateMachine → Alerts   │
│         ↑                                            ↓         │
│         └────────── Flash Buffer (24h) ──────────────┘         │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

### Fluxo Operacional Detalhado

#### **Fase de Inicialização **
1. FreeRTOS scheduler inicia
2. I2C configurado, BH1750 testado
3. DHT11 e MAX9814 inicializados
4. NVS flash verificado
5. NTP sync iniciado (se conectado a WiFi)
6. Tasks e Software Timers criadas

#### **Operação Normal (ALERT_NONE)**
1. **Timer periodicidade**:
   - DHT11: a cada 2 s
   - BH1750: a cada 10 s
   - MAX9814: a cada 1 s
2. **Processamento**:
   - Armazena amostra em buffer circular
   - Calcula média móvel de 1 min
   - Compara com limites
3. **Decisão**:
   - Se dentro → LED desligado, nenhum alerta
   - Se fora por ≥ X leituras → ALERT_WARNING
4. **Persistência**:
   - A cada 1 min: escreve agregados em NVS

#### **Alerta (ALERT_WARNNING)**
1. **Timer periodicidade**:
   - DHT11: a cada 2 s
   - BH1750: a cada 10 s
   - MAX9814: a cada 1 s
2. **Processamento**:
   - Armazena amostra em buffer circular
   - Calcula média móvel de 1 min
   - Compara com limites
   - Ativa LED amarelo
3. **Decisão**:
   - Se dentro → ALERT_NONE
   - Se fora por ≥ X leituras → ALERT_CRITICAL
4. **Persistência**:
   - A cada 1 min: escreve agregados em NVS

#### **Alerta (ALERT_CRITICAL)**
1. **Timer periodicidade**:
   - DHT11: a cada 2 s
   - BH1750: a cada 10 s
   - MAX9814: a cada 1 s
2. **Processamento**:
   - Armazena amostra em buffer circular
   - Calcula média móvel de 1 min
   - Compara com limites
   - Ativa LED vermelho e buzzer (1 pulso a cada 30s)
4. **Decisão**:
   - Se dentro → ALERT_NONE
5. **Ações do usuário**:
   - Botão clique: ativa snooze (buzzer silencia por 5 min)

---

## Documentação Técnica Detalhada

Para informações aprofundadas sobre a implementação, arquitetura de software, estruturas de dados e decisões de design, consulte **[Detalhes de Implementação](docs/implementation.md)**.

---

## Configuração de Hardware

### Pinagem dos Componentes

| Componente | GPIO/Interface | Tipo | Função |
|-----------|----------------|------|--------|
| **DHT11/22** | GPIO 23 | Digital (1-Wire) | Temperature & Humidity |
| **BH1750FVI** | I2C (SDA=21, SCL=19/20) | I2C | Light Sensor (0x23) |
| **MAX9814** | ADC Channel 5 (GPIO 33) | ADC Oneshot | Noise Level (RMS) |
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

### Configurações ADC (MAX9814)
- **Resolução**: 12-bit (0–4095)
- **Atenuação**: 11dB (para range máximo 0–3.6V)
- **Amostragem**: 1024 amostras @ 8kHz (125µs entre amostras)
- **Processamento**: RMS calculation com DC bias removal
- **Latência**: ~130ms por leitura (aquisição + cálculo)

### Diagrama de Conexão Simplificado

```
ESP32 DevKit / ESP32-S2 Saola
┌──────────────────────────────────────┐
│                                      │
│ GPIO23──→ DHT11 (Data)               │
│           • GND = GND                │
│           • VCC = 3.3V               │
│                                      │
│ GPIO22──→ Button ── GND              │
│           (Pull-up interno ativo)    │
│                                      │
│ GPIO26──→ LED Red ──[220Ω]── GND     │
│ GPIO32──→ LED Yellow ──[220Ω]── GND  │
│                                      │
│ GPIO15──→ Buzzer (PWM)               │
│          • GND = GND                 │
│          • VCC = 3.3V/5V (amplifier) │
│                                      │
│ I2C (SDA21, SCL19/20)──→ BH1750      │
│                         ├─ VCC=3.3V  │
│                         ├─ GND       │
│                         └─ ADDR=GND  │
│                                      │
│ GPIO33 (ADC5)──→ MAX9814 (OUT)       │
│                 ├─ GND               │
│                 ├─ VCC=3.3V/5V       │
│                 ├─ GAIN (float/GND)  │
│                 └─ AR (float for AGC)│
│                                      │
│ GPIO1 (TX) ↔ Serial Monitor          │
│ GPIO3 (RX)  (115200 baud)            │
│                                      │
└──────────────────────────────────────┘
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

### 2. Configuração dos Limites

Ajuste os defines em [include/env.h](include/env.h) conforme necessário:

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

/* Limites de Ruído (RMS em percentual 0-100%) */
#define NOISE_PEAK_LIMIT 40  // Limite de pico (40%)
#define NOISE_AVG_LIMIT  21  // Limite de média (21%)

/* Tolerâncias de Tempo (em amostras) */
#define DHT_WINDOW_WARNING_TOLERANCE 30   // 5 min (30 * 10s)
#define DHT_WINDOW_CRITICAL_TOLERANCE 60  // 10 min (60 * 10s)

#define LIGHT_WINDOW_WARNING_TOLERANCE 12  // 2 min (12 * 10s)
#define LIGHT_WINDOW_CRITICAL_TOLERANCE 30 // 5 min (30 * 10s)

#define NOISE_PEAK_MAX_DURATION 3        // 3 segundos
#define NOISE_WINDOW_WARNING_TOLERANCE 40 // 40 segundos

/* Intervalos de Amostragem (ms) */
#define DHT11_READ_INTERVAL_MS 2000
#define BH1750_READ_INTERVAL_MS 10000
#define NOISE_READ_INTERVAL_MS 1000  // MAX9814: ~130ms por leitura

/* Configuração de Buffer */
#define HISTORY_BUFFER_SIZE 60        // Entradas em RAM
#define NVS_HISTORY_BUFFER_SIZE 1440  // 24h @ 1 min/amostra
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
- **Logs visíveis do sistema via UART** (115200 baud):
  ```log
   (642317) DHT11: Temperature: 30.00°C, Humidity: 71.00%
   W (642317) ALERT: [WARNING] Leitura de temperatura/umidade fora dos limites seguros!
   I (642317) NOISE: Sound level: 1 (raw ADC value)
   I (643307) NOISE: Sound level: 1 (raw ADC value)
   I (644317) DHT11: Temperature: 30.00°C, Humidity: 71.00%
   W (644317) ALERT: [WARNING] Leitura de temperatura/umidade fora dos limites seguros!
   I (644317) NOISE: Sound level: 5 (raw ADC value)
   I (645307) NOISE: Sound level: 2 (raw ADC value)
   I (646317) DHT11: Temperature: 30.00°C, Humidity: 71.00%
   W (646317) ALERT: [CRITICAL] Leitura de temperatura/umidade fora dos limites seguros!
   I (646317) NOISE: Sound level: 6 (raw ADC value)
   I (647307) NOISE: Sound level: 2 (raw ADC value)
   I (648317) DHT11: Temperature: 30.00°C, Humidity: 71.00%
  ```

- **Alertas Visuais/Sonoros**:
  - LED amarelo + buzzer pulsos → temperatura / umidade / luz fora por 1–5 min
  - LED vermelho + buzzer contínuo → alarme crítico (> 5 min ou múltiplas)

- **Interação com Botão**:
  - **Primeiros 5 segundos (durante boot)**: Pressione por ≥ 2s para exportar dados em JSON e reiniciar
  - **Durante operação normal**: Pressione para ativar snooze (buzzer silencia por 10 minutos se em ALERT_CRITICAL)

#### Exportação de Dados via Botão

**Durante os primeiros 5 segundos após boot**, pressione o botão:

- **Pressionamento longo (≥ 2 segundos)**: Exporta TODOS os registros armazenados em flash via UART (formato JSON) e reinicia o dispositivo

**Exemplo de saída JSON** (via Serial Monitor a 115200 baud):
```
[
  {"timestamp": 1702473600, "temperature": 23.2, "humidity": 48.5, "lux": 2, "noise": 35},
  {"timestamp": 1702477200, "temperature": 23.1, "humidity": 49.2, "lux": 1, "noise": 32},
  ...
  {"timestamp": 1702560000, "temperature": 24.8, "humidity": 51.3, "lux": 280, "noise": 48}
]
```

---

## Configuração Avançada

### Ajuste de Histerese

Altere em [include/env.h](include/env.h):
```c
#define DHT_WINDOW_WARNING_TOLERANCE 30   // 5 min (30 * 10s)
#define DHT_WINDOW_CRITICAL_TOLERANCE 60  // 10 min (60 * 10s)
#define LIGHT_WINDOW_WARNING_TOLERANCE 12 // 2 min (12 * 10s)
#define ALERT_RESET_HYSTERESIS 3          // Leituras consecutivas OK para limpar alerta
```

- Valor baixo: alerta mais rápido, mais falsos positivos
- Valor alto: alerta mais lento, mais estável

### Modificação de Intervalo de Amostragem

Em [include/env.h](include/env.h):
```c
#define DHT11_READ_INTERVAL_MS 2000   // 2 s (mín. ~1.5 s para DHT11)
#define BH1750_READ_INTERVAL_MS 10000 // 10 s
#define NOISE_READ_INTERVAL_MS 1000   // 1 s (MAX9814: ~130ms por leitura)
```

### Persistência em Flash

Em [src/main.c](src/main.c), configure o tamanho do buffer circular:
```c
buffer = flash_buffer_init("sensors", sizeof(flash_record_t), NVS_HISTORY_BUFFER_SIZE);
// NVS_HISTORY_BUFFER_SIZE = 1440 (24h @ 1 min/amostra)
// Para 48h: altere NVS_HISTORY_BUFFER_SIZE para 2880 em env.h
```

### Sincronização NTP

Em [src/main.c](src/main.c), configure credenciais WiFi e servidor NTP:
```c
// Credenciais WiFi em include/env.h
#define WIFI_SSID "your_ssid"
#define WIFI_PASSWORD "your_password"

// No main.c, a sincronização usa defaults:
sync_time_with_ntp(WIFI_SSID, WIFI_PASSWORD, NULL, NULL);
// timezone default: "BRT3" (Brasil)
// servidor NTP default: "a.st1.ntp.br"

// Para customizar:
sync_time_with_ntp(WIFI_SSID, WIFI_PASSWORD, "UTC0", "pool.ntp.org");
```

---

## Troubleshooting

### Problema: BH1750 não responde (I2C error)
**Solução:**
1. Verifique conexões I2C (SDA, SCL, GND, VCC)
2. Confira pull-ups 4.7 kΩ nas linhas I2C

### Problema: DHT11 leituras falhando (checksum error)
**Solução:**
1. Aumente intervalo em `env.h` (DHT quer ~2 s entre leituras)
2. Adicione capacitor 100 nF entre dados e GND
3. Verifique alimentação (3.3V estável)
4. Se instável, use DHT22 (melhor tolerance)

### Problema: Buzzer não faz som
**Solução:**
1. Verifique GPIO 15 está livre (não conflita com flash)
2. Teste PWM direto: `set_buzzer(true)` em [src/alerts.c](src/alerts.c)
3. Se buzzer passivo, verifique polaridade e tensão (3.3V vs 5V)
4. Use amplificador externo se sinal fraco

### Problema: Botão não responde
**Solução:**
1. Teste com `gpio_set_level(15, 1)` (pull-up interno)
2. Verifique debounce de 50 ms em [src/button_driver.c](src/button_driver.c)
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
- [DHT11/DHT22 Datasheet](docs/datasheets/DHT11_Datasheet.pdf)
- [BH1750FVI Datasheet](docs/datasheets/BH1750FVI%20-%20Sensor%20ICs.pdf)
- [MAX9814 Microphone Amplifier](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX9814.pdf) - Sensor de ruído atual

### Documentação ESP32
- [ESP32 Technical Reference Manual](https://espressif-docs.readthedocs.io/projects/esp32-technical-reference-manual/en/latest/)
- [ESP-IDF Official Docs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [FreeRTOS on ESP32](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html)

### Protocolos de Comunicação
- [I2C Specification](https://www.nxp.com/docs/en/user-manual/UM10204.pdf)
- [1-Wire Protocol (DHT)](https://www.maximintegrated.com/en/design/technical-documents/tutorials/706.html)

---

## Licença

Este projeto está licenciado sob a **Licença MIT**. Consulte o arquivo [LICENSE](LICENSE) para mais detalhes.

---

## Contribuindo

Para contribuir com melhorias:

1. **Fork** o repositório
2. **Crie um branch** com descrição: `feature/nova-funcionalidade`
3. **Commit** mensagens claras em português
4. **Teste** em hardware
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
1. Verifique este README, [docs/implementation.md](docs/implementation.md) e [docs/](docs/) detalhadamente
2. Abra uma **Issue** no GitHub com:
   - Placa ESP32 utilizada (DevKit, Saola, etc.)
   - Logs do serial monitor
   - Configuração de limites aplicada
   - Passos para reproduzir problema

---

**Última atualização:** Dezembro 2025  
**Status:** MVP em produção de testes
