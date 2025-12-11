# Sistema de Monitoramento Ambiental para Biotério

Este projeto implementa um sistema embarcado de monitoramento ambiental desenvolvido para biotérios (vivários de animais de laboratório), utilizando ESP32 com FreeRTOS. O sistema monitora temperatura, umidade, luminosidade e níveis de ruído, armazenando dados em memória flash não-volátil e gerando alertas visuais e sonoros quando parâmetros ultrapassam limites seguros.

## Contexto e Objetivo do Projeto

### Requisitos do Sistema
O projeto atende aos seguintes requisitos estabelecidos para monitoramento de biotérios:

- **Monitoramento Contínuo**: Leitura periódica de sensores ambientais (temperatura, umidade, luz, ruído)
- **Armazenamento Persistente**: Dados salvos em flash NVS (Non-Volatile Storage) do ESP32
- **Sistema de Alertas**: Indicação visual (LEDs) e sonora (buzzer) de condições fora dos limites
- **Histórico de Dados**: Média móvel de 60 amostras calculada a cada minuto
- **Dump de Dados**: Extração de dados via UART em formato JSON mediante pressão longa de botão
- **Sincronização Temporal**: Timestamping via NTP (Network Time Protocol) com WiFi

### Faixas de Operação

**Limites de Temperatura:**
- Zona Segura: 18°C a 26°C
- Zona de Alerta (Warning): 16°C a 18°C ou 26°C a 28°C
- Zona Crítica: < 16°C ou > 28°C

**Limites de Umidade:**
- Zona Segura: 40% a 70%
- Zona de Alerta (Warning): 30% a 40% ou 70% a 80%
- Zona Crítica: < 30% ou > 80%

**Outros Sensores:**
- **Luminosidade**: 0 a 65535 lux (sensor BH1750FVI)
- **Ruído**: 0 a 4095 (ADC 12-bit do ESP32)

## Funcionalidades Implementadas

### Sensores e Monitoramento
- **DHT11**: Sensor de temperatura e umidade digital (leitura a cada 2s)
- **BH1750FVI**: Sensor de luminosidade I2C de alta precisão (leitura a cada 10s)
- **KY-037**: Sensor de som/microfone analógico (leitura a cada 1s)
- **Sistema de Monitores**: Cada sensor possui task dedicada com timer periódico em FreeRTOS

### Sistema de Alertas
- **3 Estados do Sistema**:
  - `STATE_NORMAL`: LED verde aceso, condições dentro dos limites
  - `STATE_WARNING`: LED amarelo piscando (2 Hz), temperatura ou umidade próximas aos limites
  - `STATE_CRITICAL`: LED vermelho + buzzer intermitente, violação de limites críticos
- **Lógica de Histerese**: Requer 3 leituras consecutivas fora dos limites para mudar de estado (previne alarmes falsos)

### Armazenamento e Histórico
- **Flash Buffer Circular**: 1440 posições (24h com 1 amostra/minuto)
- **Compactação de Dados**: Estrutura `compact_sensor_read_t` de 5 bytes por amostra
- **Média Móvel**: Cálculo de média de 60 amostras (últimos 60 minutos) em RAM
- **Timestamping**: Cada amostra tem timestamp Unix desde boot ou NTP

### Interface e Controle
- **Botão de Navegação**: GPIO 22 com debouncing e detecção de eventos
  - **Click Simples**: Evento de teste (atualmente dispara reboot)
  - **Pressão Longa (>2s)**: Dump completo de dados via UART em JSON + reset
- **UART JSON**: Protocolo de comunicação para extração de dados históricos
- **WiFi + NTP**: Sincronização automática de horário na inicialização

## Arquitetura do Sistema

### Visão Geral das Tasks FreeRTOS

```
┌────────────────────────────────────────────────────────────┐
│                     app_main (Task)                        │
│  - Inicializa sistema (NVS, WiFi, NTP, sensores)          │
│  - Aguarda eventos de botão (Task Notification)           │
│  - Cria tasks de monitoramento para cada sensor           │
└────────────────────────────────────────────────────────────┘
                            │
            ┌───────────────┼───────────────┐
            ▼               ▼               ▼
┌──────────────────┐ ┌──────────────┐ ┌────────────────┐
│ Sensor Monitor   │ │ Sensor       │ │ Sensor Monitor │
│  (DHT11 Task)    │ │ Monitor      │ │ (BH1750 Task)  │
│  - 2s interval   │ │ (KY-037)     │ │  - 10s interval│
│  - Temp & Humid  │ │  - 1s        │ │  - Luminosity  │
└──────────────────┘ └──────────────┘ └────────────────┘
            │               │               │
            └───────────────┼───────────────┘
                            ▼
            ┌──────────────────────────────┐
            │   Callbacks: save_dht11()    │
            │   save_ky037(), save_bh1750()│
            │  - Atualiza current_read     │
            │  - Salva no sensor_history   │
            └──────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────────┐
│            Sensor History System (RAM)                     │
│  - Buffer circular de 60 amostras compactadas             │
│  - Cálculo de média móvel a cada 60s                      │
│  - Quando cheio, salva no flash_buffer                    │
└────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌────────────────────────────────────────────────────────────┐
│         Flash Buffer System (NVS Persistent)               │
│  - 1440 registros flash_record_t (24h de histórico)       │
│  - Cada registro: timestamp + compact_sensor_read_t       │
│  - Sobrevive a resets e power-off                         │
└────────────────────────────────────────────────────────────┘
                            │
                            ▼
            ┌──────────────────────────────┐
            │   Alert Logic Task           │
            │  - Recebe sensor readings    │
            │  - Calcula estado do sistema │
            │  - Controla LEDs e Buzzer    │
            └──────────────────────────────┘
```

### Fluxo de Dados

1. **Aquisição**: Cada sensor é lido periodicamente por task dedicada (Software Timer + Task Notification)
2. **Callback**: Função `save_<sensor>()` atualiza estrutura `current_read` com novos valores
3. **Histórico RAM**: Dados salvos em buffer circular de 60 amostras (`sensor_history`)
4. **Média Móvel**: Timer de 60s calcula média das últimas 60 amostras
5. **Persistência**: Média salva em `flash_buffer` (NVS) com timestamp
6. **Alertas**: Task de alerta avalia condições e atualiza LEDs/buzzer conforme estado

### Estruturas de Dados Principais

#### `full_sensor_read_t` (RAM - Não Compactado)
```c
typedef struct {
    float temperature;   // °C
    float humidity;      // %
    float lux;          // lx
    uint16_t noise_level;  // ADC raw (0-4095)
} full_sensor_read_t;
```

#### `compact_sensor_read_t` (5 bytes - Compactado)
```c
typedef struct {
    uint8_t temperature : 6;   // 0-50°C, step 1.0
    uint8_t humidity : 7;      // 0-100%, step 1.0
    uint16_t lux : 15;         // 0-16384 lux, step 0.5
    uint16_t noise_level : 12; // 0-4096, step 2.0
} compact_sensor_read_t;
```

#### `flash_record_t` (9 bytes - Armazenamento Flash)
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
- **GPIO 21/22**: I2C (SDA/SCL) para BH1750FVI (luminosidade)
  - Endereço I2C: `0x23` ou `0x5C` (configurável via pino ADDR)
- **GPIO 33**: KY-037 Analog Out (ADC1_CH5 - nível de ruído)
- **GPIO 34**: KY-037 Digital Out (opcional, não utilizado)

#### Indicadores e Controle
<!-- - **GPIO 16**: LED Verde (Estado Normal) -->
- **GPIO 17**: LED Amarelo (Estado Warning)
- **GPIO 5**: LED Vermelho (Estado Crítico)
- **GPIO 18**: Buzzer PWM (LEDC Channel 0, 4 kHz)
- **GPIO 22**: Botão de Navegação (pull-up interno, falling edge interrupt)

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
├── main/                    # Código principal
│   ├── main.c               # Entry point, inicialização, app_main()
│   └── idf_component.yml    # Dependências do componente
│
├── include/                 # Headers públicos
│   ├── sensor_base.h        # Interface base para sensores
│   ├── sensor_monitor.h     # Sistema de monitoramento periódico
│   ├── sensor_history.h     # Histórico em RAM e compactação
│   ├── flash_record.h       # Estrutura de registro na flash
│   ├── dht11_sensor.h       # Driver DHT11
│   ├── bh1750fvi_sensor.h   # Driver BH1750FVI
│   ├── KY-037_sensor.h      # Driver KY-037
│   ├── alerts.h             # Sistema de alertas (LEDs + buzzer)
│   ├── button_driver.h      # Driver de botão com debouncing
│   ├── time_sync.h          # Sincronização NTP via WiFi
│   └── uart_json_handler.h  # Dump de dados JSON via UART
│
├── src/                     # Implementações
│   ├── sensor_monitor.c
│   ├── sensor_history.c
│   ├── dht11_sensor.c
│   ├── bh1750fvi_sensor.c
│   ├── KY-037_sensor.c
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

## Como Usar

### 1. Pré-requisitos

#### Software Necessário
- **PlatformIO Core** ou **PlatformIO IDE** (extensão VS Code recomendada)
- **Python 3.8+** (para ferramentas ESP-IDF)
- **Driver USB-UART**: CP210x ou CH340 (dependendo da placa)

#### Extensões VS Code Recomendadas
- [PlatformIO IDE](https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide)
- [C/C++ Extension Pack](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack)
- [ESP-IDF](https://marketplace.visualstudio.com/items?itemName=espressif.esp-idf-extension) (opcional)

### 2. Montagem do Hardware

#### Conexões dos Sensores

**DHT11 (Temperatura e Umidade):**
```
DHT11 VCC  → ESP32 3.3V
DHT11 DATA → ESP32 GPIO 23
DHT11 GND  → ESP32 GND
(Resistor pull-up de 10kΩ entre DATA e VCC recomendado)
```

**BH1750FVI (Luminosidade):**
```
BH1750 VCC  → ESP32 3.3V
BH1750 GND  → ESP32 GND
BH1750 SCL  → ESP32 GPIO 21 (I2C Clock)
BH1750 SDA  → ESP32 GPIO 19 (I2C Data)
BH1750 ADDR → GND (endereço I2C 0x23) ou VCC (0x5C)
```

**KY-037 (Sensor de Som):**
```
KY-037 VCC → ESP32 5V ou 3.3V
KY-037 GND → ESP32 GND
KY-037 AO  → ESP32 GPIO 33 (ADC1_CH5)
```

#### Indicadores Visuais e Sonoros

**LEDs (com resistores de 220Ω em série):**
```
LED Amarelo (+) → GPIO 17 → Resistor → GND
LED Vermelho(+) → GPIO 5  → Resistor → GND
```

**Buzzer Ativo (ou Passivo com PWM):**
```
Buzzer (+) → GPIO 18
Buzzer (-) → GND
(Para buzzer passivo, o sistema gera PWM de 4 kHz)
```

**Botão de Navegação:**
```
Um terminal → GPIO 22
Outro terminal → GND
(Pull-up interno habilitado no código)
```

### 3. Configuração do Projeto

#### Compilação e Upload

**Via PlatformIO CLI:**
```bash
# Compilar para ESP32 WROOM
pio run -e upesy_wroom

# Fazer upload
pio run -e upesy_wroom -t upload

# Monitorar serial (115200 baud)
pio device monitor -b 115200
```

**Via VS Code PlatformIO:**
1. Abrir projeto no VS Code
2. Selecionar environment: `upesy_wroom` ou `esp32-s2-saola-1`
3. Clicar em "Build" (✓) na barra inferior
4. Clicar em "Upload" (→) para gravar
5. Clicar em "Serial Monitor" (🔌) para ver logs

#### Configuração WiFi

Editar em `src/main.c` a chamada de `sync_time_with_ntp()`:

```c
// Linha ~98 em main.c
ret = sync_time_with_ntp("SEU_SSID", "SUA_SENHA", NULL, NULL);
```

**Nota**: Se WiFi não estiver disponível, o sistema funcionará normalmente mas timestamps serão relativos ao boot (não Unix time absoluto).

### 4. Operação do Sistema

#### Inicialização
1. Conecte a placa ao computador via USB
2. O sistema iniciará automaticamente após ~5 segundos
3. Sequência de inicialização:
   - Inicialização NVS (flash)
   - Criação do flash buffer (1440 registros)
   - Inicialização do botão com callback
   - **Janela de 5s para teste de botão** (pressão longa reinicia)
   - Conexão WiFi e sincronização NTP
   - Inicialização dos sensores (DHT11, BH1750, KY-037)
   - Início do monitoramento contínuo

#### Monitoramento Contínuo

O sistema opera automaticamente:

- **DHT11**: Lê temperatura/umidade a cada 2 segundos
- **KY-037**: Lê nível de ruído a cada 1 segundo
- **BH1750**: Lê luminosidade a cada 10 segundos
- **Média Móvel**: Calcula média de 60 amostras a cada 60 segundos
- **Persistência**: Salva média no flash automaticamente

#### Estados dos Indicadores

**LED Verde Aceso (Estado Normal):**
- Temperatura: 18°C a 26°C
- Umidade: 40% a 70%
- Sistema operando dentro dos limites seguros

**LED Amarelo Piscando 2 Hz (Estado Warning):**
- Temperatura: 16-18°C ou 26-28°C
- Umidade: 30-40% ou 70-80%
- Condições próximas aos limites, atenção necessária
- Requer 3 leituras consecutivas para ativar

**LED Vermelho + Buzzer Intermitente (Estado Crítico):**
- Temperatura: < 16°C ou > 28°C
- Umidade: < 30% ou > 80%
- Violação de limites críticos, ação imediata requerida
- Buzzer toca em padrão intermitente (200ms on/off)

#### Extração de Dados

**Pressione o botão por mais de 2 segundos:**

1. Sistema para o monitoramento
2. LED indicador pisca (confirmação de comando)
3. Dados são enviados via UART em formato JSON:
   ```json
   {"timestamp":1234567890,"temp":24,"hum":55,"lux":450.5,"noise":120}
   {"timestamp":1234567950,"temp":24,"hum":56,"lux":448.0,"noise":118}
   ...
   ```
4. Após dump completo:
   - Flash é limpo (NVS erased)
   - Sistema reinicia automaticamente
   - Monitoramento recomeça do zero

**Captura via Serial:**
```bash
# Linux/macOS
pio device monitor -b 115200 > dados_bioterio.json

# Windows (PowerShell)
pio device monitor -b 115200 | Out-File dados_bioterio.json
```

### 5. Análise de Dados (Pós-Coleta)

Os dados JSON podem ser processados com Python, R, MATLAB ou qualquer ferramenta de análise:

**Exemplo Python:**
```python
import json
import pandas as pd

# Ler arquivo JSON (uma linha por registro)
with open('dados_bioterio.json', 'r') as f:
    records = [json.loads(line) for line in f]

# Criar DataFrame
df = pd.DataFrame(records)
df['datetime'] = pd.to_datetime(df['timestamp'], unit='s')

# Análise estatística
print(df[['temp', 'hum', 'lux', 'noise']].describe())

# Plotagem
df.plot(x='datetime', y=['temp', 'hum'], subplots=True)
```

## Configuração Avançada

### Ajuste de Limites de Alerta

Editar em `include/alerts.h`:

```c
// Limites de Temperatura
#define TEMP_MIN 18.0
#define TEMP_MAX 26.0
#define WARN_OFFSET_TEMP 2.0  // Warning zone offset

// Limites de Umidade
#define HUM_MIN  40.0
#define HUM_MAX  70.0
#define WARN_OFFSET_HUM  10.0
```

### Ajuste de Intervalos de Leitura

Editar em `src/main.c`:

```c
#define DHT11_READ_INTERVAL_MS 2000   // Temperatura/umidade
#define BH1750_READ_INTERVAL_MS 10000 // Luminosidade
#define KY037_READ_INTERVAL_MS 1000   // Ruído
```

### Tamanho do Buffer Flash

Editar em `src/main.c` (linha ~77):

```c
// Atual: 1440 registros (24h com 1 amostra/min)
buffer = flash_buffer_init("sensors", sizeof(flash_record_t), 1440);

// Exemplo: 7 dias de dados (10080 registros)
buffer = flash_buffer_init("sensors", sizeof(flash_record_t), 10080);
```

**Nota**: Verificar capacidade da partição NVS antes de aumentar significativamente.

### Partição NVS Customizada

[LACUNA: Procedimento para criar custom partition table no ESP-IDF com maior espaço para NVS]

## Troubleshooting

### Problemas Comuns

#### Sensor DHT11 Não Responde
**Sintoma**: Logs mostram "DHT11 timeout" ou leituras sempre falham

**Solução**:
- Verificar conexão do pino de dados (GPIO 23)
- Adicionar resistor pull-up de 10kΩ entre DATA e VCC
- DHT11 requer intervalo mínimo de 1s entre leituras (atual: 2s)

#### BH1750 Não Detectado
**Sintoma**: "I2C device not found at 0x23"

**Solução**:
- Verificar conexões SDA (GPIO 21) e SCL (GPIO 22)
- Testar endereço alternativo 0x5C (conectar ADDR a VCC)
- Verificar pull-ups no barramento I2C (2.2kΩ - 10kΩ)
- Executar I2C scan: [LACUNA: comando de scan I2C]

#### WiFi Não Conecta
**Sintoma**: Sistema trava por minutos na inicialização

**Solução**:
- Verificar SSID e senha corretos em `sync_time_with_ntp()`
- Verificar se rede é 2.4 GHz (ESP32 não suporta 5 GHz)
- Comentar linha de `sync_time_with_ntp()` para operar sem WiFi
- Timestamps serão relativos ao boot, não Unix time absoluto

#### Flash Buffer Cheio Rapidamente
**Sintoma**: "Flash buffer full" nos logs após poucas horas

**Solução**:
- Aumentar tamanho do buffer em `flash_buffer_init()`
- Ajustar período de salvamento (atual: 1 amostra/minuto)
- Fazer dump periódico de dados antes de encher

#### Buzzer Não Funciona
**Sintoma**: LED vermelho acende mas sem som

**Solução**:
- Verificar polaridade do buzzer (+ no GPIO 18)
- Buzzer ativo: deve funcionar direto
- Buzzer passivo: requer PWM (implementado, frequência 4 kHz)
- Testar buzzer com multímetro (deve mostrar ~3.3V quando ativo)

#### Botão Não Detecta Pressão Longa
**Sintoma**: Botão só funciona nos primeiros 5 segundos

**Solução**:
- Após inicialização, botão ainda deve funcionar
- Verificar conexão GPIO 22 a GND via botão
- [LACUNA: comportamento esperado do botão após janela de teste inicial]

### Debug via Serial

Habilitar logs detalhados em `platformio.ini`:

```ini
monitor_filters = 
    esp32_exception_decoder
    log2file
    colorize
```

Níveis de log (editar em código):
```c
esp_log_level_set("*", ESP_LOG_INFO);     // Padrão
esp_log_level_set("sensor_monitor", ESP_LOG_DEBUG);  // Verbose para módulo específico
```

## Desenvolvimento e Extensão

### Adicionando Novos Sensores

1. **Criar header em `include/<sensor_name>.h`**:
   ```c
   #ifndef SENSOR_NAME_H
   #define SENSOR_NAME_H
   #include "sensor_base.h"
   
   void sensor_name_init(sensor_base_t *sensor, ...);
   
   #endif
   ```

2. **Implementar em `src/<sensor_name>.c`**:
   ```c
   void sensor_name_init(sensor_base_t *sensor, ...) {
       sensor->name = "SensorName";
       sensor->read = sensor_name_read;
       sensor->init = sensor_name_init_func;
       // ...
   }
   ```

3. **Criar monitor em `main.c`**:
   ```c
   sensor_base_t new_sensor = {0};
   sensor_name_init(&new_sensor, ...);
   
   sensor_monitor_t *monitor = new_sensor_monitor(
       &new_sensor, INTERVAL_MS, sizeof(data_t), "name", callback);
   
   start_sensor_monitoring(monitor);
   ```

4. **Atualizar `current_read` no callback**:
   ```c
   void save_new_sensor(sensor_base_t *sensor, void *data) {
       my_data_t *value = (my_data_t *)data;
       // Atualizar campo relevante em current_read
       save_sensor_read(&current_read);
   }
   ```

### Modificando Compactação de Dados

[LACUNA: Procedimento para modificar bit-fields em `compact_sensor_read_t` e macros de conversão em `sensor_history.h`]

### Contribuindo

Para contribuir com o projeto:

1. **Fork** o repositório
2. **Crie branch** descritivo: `feature/novo-sensor` ou `fix/bug-wifi`
3. **Implemente** mudanças com commits atômicos
4. **Teste** em hardware real antes de submeter
5. **Documente** novas funcionalidades no README
6. **Envie Pull Request** com descrição detalhada

### Padrões de Código

- **Nomenclatura**: snake_case para funções e variáveis, PascalCase para tipos
- **Indentação**: 4 espaços (não tabs)
- **Comentários**: Doxygen style para funções públicas
- **Logs**: Usar `ESP_LOG*()` em vez de `printf()`
- **Erros**: Sempre verificar retorno `esp_err_t` com `ESP_ERROR_CHECK()` ou tratamento explícito

## Limitações Conhecidas

1. **Capacidade de Armazenamento**: NVS padrão ~24KB, limita histórico a ~2500 registros
2. **Precisão Temporal sem WiFi**: Timestamps relativos ao boot, não há RTC externo
3. **Sensores DHT11**: Resolução de 1°C e 1% (considerar DHT22 para maior precisão)
4. **Ruído ADC**: KY-037 sensível a ruído elétrico, filtro digital recomendado
5. **[LACUNA: Outras limitações do projeto]**

## Trabalhos Futuros

- [ ] Implementar servidor HTTP para visualização em tempo real
- [ ] Adicionar suporte a MQTT para integração IoT
- [ ] Implementar log de eventos críticos separado
- [ ] Adicionar suporte a múltiplos pontos de monitoramento (rede de sensores)
- [ ] Implementar calibração automática de sensores
- [ ] Adicionar RTC externo (DS3231) para timestamping preciso sem WiFi

## Licença

Este projeto está licenciado sob a **MIT License**. Consulte o arquivo `LICENSE` para detalhes.

## Referências

### Datasheets
- [BH1750FVI - Sensor de Luminosidade](docs/BH1750FVI%20-%20Sensor%20ICs.pdf)
- [DHT11 - Sensor de Temperatura e Umidade](docs/DHT11_Datasheet.pdf)
- [KY-037 - Sensor de Som](docs/KY-037-datasheet.pdf)
- [CZN-15E - Buzzer Piezoelétrico](docs/CZN-15E.pdf)

### Documentação Técnica
- [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [FreeRTOS Kernel Guide](https://www.freertos.org/Documentation/RTOS_book.html)
- [NVS (Non-Volatile Storage) Library](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)
- [ESP32 ADC Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html)

### Bibliotecas Utilizadas
- [esp-idf-lib](https://github.com/UncleRus/esp-idf-lib) - Coleção de drivers para ESP-IDF
- [DHT Library for ESP-IDF](https://github.com/UncleRus/esp-idf-lib/tree/master/components/dht)

---

**Desenvolvido como projeto acadêmico de Sistemas Embarcados - [LACUNA: instituição e período]**