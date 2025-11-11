# Sistema Embarcado - Projeto I2C/SPI com PCF8591 e MAX7219

Este projeto implementa um sistema embarcado avançado utilizando o microcontrolador STM32L476RG, integrando múltiplos protocolos de comunicação (I2C, SPI, UART) e periféricos. O sistema permite ler valores analógicos do módulo ADC/DAC PCF8591, controlar display LED 8x8 MAX7219, e processar comandos via interface UART, utilizando uma arquitetura modular com drivers especializados.

## Funcionalidades

### Comunicação e Protocolos
- **Interface I2C**: Comunicação com módulo PCF8591 ADC/DAC
- **Interface SPI**: Controle de display LED 8x8 MAX7219 com DMA
- **Interface UART**: Recepção e transmissão de comandos via USART2
- **DMA**: Transferências assíncronas para UART, SPI e I2C

### Processamento de Dados
- **Processamento de Comandos**: Sistema robusto baseado em strings com buffer circular
- **Leitura de Canais Analógicos**: 4 canais ADC do PCF8591 (AIN0-AIN3, 8-bit, 0-255)
- **Controle de DAC**: Saída analógica configurável (0-255) no PCF8591
- **Detecção de Tendências**: Comparação de leituras consecutivas com indicação visual

### Display e Visualização
- **Display LED 8x8**: MAX7219 controlado via SPI
- **Ícones Customizados**: Representações visuais para Temperatura (T), Tensão (V), e Luz (L)
- **Indicadores de Tendência**: Símbolos +/- para aumento/diminuição de valores
- **Atualização Periódica**: Display atualizado automaticamente via timer
- **Display Alternado**: Alternância entre ícone do sensor e tendência

### Arquitetura do Sistema
- **Máquina de Estados**: 8 estados para controle de fluxo robusto
- **Drivers Modulares**: Arquitetura separada por periférico
- **Comunicação Assíncrona**: Operações não-bloqueantes com callbacks
- **Buffer Circular**: Gerenciamento eficiente de dados UART (1024 bytes)
- **Economia de Energia**: Uso de WFI (Wait For Interrupt) entre operações

## Comandos Disponíveis

O sistema utiliza **comandos baseados em strings** para controle via UART.

### Leitura de Canais ADC
- **`Read_AIN0`**: Lê o canal AIN0 e retorna valor via UART
- **`Read_AIN1`**: Lê o canal AIN1 e retorna valor via UART
- **`Read_AIN2`**: Lê o canal AIN2 e retorna valor via UART
- **`Read_AIN3`**: Lê o canal AIN3 e retorna valor via UART
- **Resposta**: `AIN<N>: <valor>` (onde valor está entre 0-255)

### Controle do DAC
- **`Set_DAC_<VALOR>`**: Define valor do DAC (0-255)
  - Exemplo: `Set_DAC_128` configura DAC para valor 128
  - Exemplo: `Set_DAC_255` configura DAC para valor máximo
- **Resposta**: `Valor do DAC: <valor>`

### Comandos de Display (Novos)
Estes comandos ativam monitoramento contínuo com visualização no display LED 8x8:

- **`Temp`**: Exibe ícone de temperatura (T) e monitora sensor no canal AIN1
  - Display alterna entre ícone T e indicador de tendência (+/-)
  - Timer atualiza periodicamente a leitura
  
- **`Volt`**: Exibe ícone de tensão (V) e monitora sensor no canal AIN3
  - Display alterna entre ícone V e indicador de tendência (+/-)
  - Ideal para monitoramento de voltagem
  
- **`LDR`**: Exibe ícone de luz (L) e monitora sensor LDR no canal AIN0
  - Display alterna entre ícone L e indicador de tendência (+/-)
  - Detecta variações de luminosidade

### Formato dos Comandos
- Todos os comandos devem ser terminados com `\n` (newline) ou `\r` (carriage return)
- Buffer circular de 1024 bytes para comandos longos ou múltiplos comandos
- Comandos inválidos retornam mensagem: `Unknown command: <comando>`

## Arquitetura do Sistema

O sistema implementa uma **máquina de estados com 8 estados** para gerenciar a comunicação entre múltiplos periféricos e protocolos de forma eficiente e robusta.

### Diagrama Geral do Sistema
![Diagrama de Estados do Sistema](docs/images/state_machine.jpg)

### Processamento de Comandos
![Diagrama de Estados dos Comandos](docs/images/Diagrama%20de%20estados%20cmd.svg)

*Documentação detalhada disponível em [docs/state_machine_diagram.md](docs/state_machine_diagram.md)*

### Máquina de Estados (8 Estados):

1. **STATE_1 (Idle)**: Estado inicial, processa comandos UART através do `cmd_tick()`
2. **STATE_2 (Config ADC)**: Configura canal ADC do PCF8591 via I2C
3. **STATE_3 (Read ADC)**: Realiza leitura do canal ADC configurado
4. **STATE_4 (UART TX)**: Transmite resposta formatada via UART
5. **STATE_5 (Write DAC)**: Configura valor do DAC no PCF8591
6. **STATE_6 (Display Config)**: Prepara buffer de display e configura canal para monitoramento
7. **STATE_7 (Monitor Read)**: Lê sensor e determina tendência (+/-)
8. **STATE_8 (Update Display)**: Atualiza display LED 8x8 via SPI com DMA

### Arquitetura Modular:

O sistema foi refatorado para uma arquitetura modular com drivers especializados:

#### **circular_buffer** (circular_buffer.c/h)
- Estrutura de dados genérica para buffers circulares
- Capacidade: 1024 bytes configurável
- Operações: push, pop, empty, full, free_space
- Thread-safe para uso com interrupções

#### **cmd_driver** (cmd_driver.c/h)
- Driver dedicado para processamento de comandos UART
- Buffer circular interno de 1024 bytes
- Máquina de estados: IDLE → RECEIVING → READY
- Detecção automática de terminadores (\n, \r)
- Callback assíncrono quando comando completo é recebido
- Integração transparente com DMA UART

#### **PCF8591_driver** (PCF8591_driver.c/h)
- Driver I2C para módulo ADC/DAC PCF8591
- Operações assíncronas com callbacks
- Funções: Init, set_channel, read_analog, write_dac
- Buffer de dados para 4 canais ADC
- Suporte para múltiplas instâncias I2C

#### **MAX7219_driver** (MAX7219_driver.c/h)
- Driver SPI para display LED 8x8 MAX7219
- Buffer de tela (8 bytes para 8 linhas)
- Controle de CS (Chip Select) via GPIO
- Configuração de brilho, modo scan, decode mode
- Atualização assíncrona com DMA e callback
- Funções: Init, Write, UpdateScreen

### Fluxos de Operação Principais:

#### Leitura de ADC (Read_AIN):
```
CMD UART → STATE_1 (parse) → STATE_2 (config I2C) → STATE_3 (read I2C) 
→ STATE_4 (UART TX) → STATE_1 (idle)
```

#### Configuração DAC (Set_DAC):
```
CMD UART → STATE_1 (parse) → STATE_5 (write DAC) → STATE_4 (confirm) 
→ STATE_1 (idle)
```

#### Monitoramento com Display (Temp/Volt/LDR):
```
CMD UART → STATE_1 (parse) → STATE_6 (prep display + config ADC) 
→ STATE_7 (read + trend) → STATE_8 (update SPI) → STATE_1 (idle)
↓
Timer → STATE_8 (periodic update) → STATE_1 (idle)
```

### Características Avançadas:

- **Comunicação Assíncrona**: Todas operações I2C, SPI e UART utilizam DMA/IT
- **Callbacks HAL**: Sincronização precisa entre periféricos e estados
- **Transições Atômicas**: Funções `set_system_state()` e `get_system_state()`
- **WFI**: Wait For Interrupt para economia de energia
- **Timer Periódico**: TIM2 dispara atualizações automáticas do display
- **Detecção de Tendências**: Comparação de leituras consecutivas com feedback visual
- **Validação de Comandos**: Parser robusto com mensagens de erro claras

## Hardware Requerido

- **Microcontrolador**: STM32L476RG (Nucleo-L476RG)
- **Módulo ADC/DAC**: PCF8591 (endereço I2C: 0x48)
  - Conexões I2C: SDA e SCL conectados ao I2C3 do STM32
  - 4 canais ADC (A0-A3) de 8-bit (0-255)
  - 1 canal DAC de 8-bit (0-255)
- **Display LED**: MAX7219 com matriz LED 8x8
  - Conexões SPI: MOSI, SCK conectados ao SPI1 do STM32
  - CS: GPIO PA4 (configurável)
  - Alimentação: 5V
- **UART**: USART2 para comunicação serial (115200 baud, 8N1)
- **Timer**: TIM2 para atualização periódica do display
- **DMA**: Canais DMA para I2C3, SPI1 e USART2

## Estrutura do Projeto

```
.
├── Core/
│   ├── Inc/                  # Arquivos de cabeçalho
│   │   ├── main.h           # Definições principais e protótipos
│   │   ├── circular_buffer.h # API do buffer circular
│   │   ├── cmd_driver.h     # API do driver de comandos UART
│   │   ├── PCF8591_driver.h # API do driver I2C PCF8591
│   │   ├── MAX7219_driver.h # API do driver SPI MAX7219
│   │   ├── stm32l4xx_hal_conf.h # Configuração HAL
│   │   └── stm32l4xx_it.h   # Tratadores de interrupção
│   └── Src/                 # Código-fonte
│       ├── main.c           # Programa principal e máquina de estados
│       ├── circular_buffer.c # Implementação do buffer circular
│       ├── cmd_driver.c     # Implementação do driver de comandos
│       ├── PCF8591_driver.c # Implementação do driver PCF8591
│       ├── MAX7219_driver.c # Implementação do driver MAX7219
│       ├── stm32l4xx_hal_msp.c # Inicialização MSP
│       ├── stm32l4xx_it.c   # Implementação das interrupções
│       └── system_stm32l4xx.c # Inicialização do sistema
├── Drivers/                 # Drivers HAL e CMSIS da ST
│   ├── STM32L4xx_HAL_Driver/ # Biblioteca HAL
│   └── CMSIS/               # CMSIS Core e Device
├── docs/                    # Documentação do projeto
│   ├── state_machine_diagram.md # Documentação detalhada dos estados
│   └── images/              # Diagramas e imagens
│       ├── state_machine.jpg # Diagrama principal de estados
│       └── Diagrama de estados cmd.svg # Diagrama do processador de comandos
├── build/                   # Diretório de saída da compilação
├── Makefile                 # Sistema de build
├── STM32L476XX_FLASH.ld     # Script de linker
├── startup_stm32l476xx.s    # Arquivo de inicialização
└── emebedded_sys_2025.2.ioc # Configuração do STM32CubeMX
```

## Como Usar

### 1. Configuração Inicial
Após compilar e gravar o firmware:

1. Conecte o módulo PCF8591 ao STM32L476RG via I2C3 (SDA/SCL)
2. Conecte o display MAX7219 ao STM32L476RG via SPI1 (MOSI/SCK/CS)
3. Conecte sensores aos canais ADC do PCF8591:
   - AIN0: Sensor LDR (Luz)
   - AIN1: Sensor de Temperatura
   - AIN2: Livre
   - AIN3: Sensor de Tensão
4. Abra um terminal serial (115200 baud, 8N1)
5. Sistema iniciará no STATE_1 (idle) aguardando comandos

### 2. Lendo Canais ADC
Para ler um canal analógico específico, envie o comando seguido de Enter:
```
> Read_AIN0
AIN0: 128

> Read_AIN2
AIN2: 255
```

### 3. Controlando o DAC
Para definir o valor do DAC (0-255):
```
> Set_DAC_128
Valor do DAC: 128

> Set_DAC_0
Valor do DAC: 0
```

### 4. Monitoramento com Display Visual (Novo)

#### Monitoramento de Temperatura:
```
> Temp
```
- Display mostra ícone "T"
- Sistema lê periodicamente o canal AIN1
- Display alterna entre ícone T e indicador de tendência (+/-)
- Atualização automática via timer

#### Monitoramento de Tensão:
```
> Volt
```
- Display mostra ícone "V"
- Sistema lê periodicamente o canal AIN3
- Indicador visual de aumento/diminuição de tensão
- Ideal para monitorar alimentação ou bateria

#### Monitoramento de Luz (LDR):
```
> LDR
```
- Display mostra ícone "L"
- Sistema lê periodicamente o canal AIN0
- Detecta variações de luminosidade
- Indicador +/- mostra se ambiente está clareando ou escurecendo

### 5. Indicadores de Tendência
- **Símbolo (+)**: Valor aumentou desde última leitura
- **Símbolo (-)**: Valor diminuiu desde última leitura
- Display alterna automaticamente entre ícone do sensor e tendência
- Frequência de atualização controlada por TIM2

### 6. Comandos Inválidos
O sistema valida todos os comandos e retorna feedback em caso de erro:
```
> InvalidCommand
Unknown command: InvalidCommand
```

## Configuração dos Periféricos

### PCF8591 (ADC/DAC via I2C3)
- **Endereço I2C**: 0x48 (padrão)
- **Canais ADC**: A0, A1, A2, A3 (8-bit, 0-255)
- **DAC**: Saída analógica de 8-bit (0-255)
- **Alimentação**: 3.3V ou 5V
- **Modo de Operação**: 4 canais single-ended
- **Clock I2C**: 100 kHz (Standard Mode)

### MAX7219 (Display LED 8x8 via SPI1)
- **Interface**: SPI Mode 0 (CPOL=0, CPHA=0)
- **Clock SPI**: Até 10 MHz
- **CS (Chip Select)**: GPIO PA4
- **Matriz LED**: 8x8 pixels
- **Brilho**: Configurável (0-15)
- **Modo Decodificação**: Desabilitado (controle direto de pixels)
- **Scan Limit**: 8 dígitos (todas as linhas ativas)

### UART (USART2)
- **Baudrate**: 115200 bps
- **Data bits**: 8
- **Parity**: None
- **Stop bits**: 1
- **Flow control**: None
- **DMA**: RX e TX habilitados

### Timer (TIM2)
- **Função**: Atualização periódica do display
- **Período**: Configurável via prescaler e ARR
- **Modo**: Interruption mode
- **Função**: Dispara STATE_8 quando em STATE_1

## Pré-requisitos

Certifique-se de que as seguintes ferramentas estão instaladas no seu sistema:

- GCC ARM toolchain (`gcc-arm-none-eabi`)
- Make
- CMake
- Ferramentas ST-Link (`stlink-tools`)

Você pode instalar essas dependências executando o script `setup_env.sh`:

```bash
sudo setup_env.sh
```

## Ambiente de Desenvolvimento

Para configurar o ambiente de desenvolvimento no Visual Studio Code, as seguintes extensões são recomendadas:

- [**C/C++ Extension Pack**](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools-extension-pack): Fornece suporte para desenvolvimento em C/C++.
- [**CMake Tools**](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools): Suporte para projetos baseados em CMake.
- [**Makefile Tools**](https://marketplace.visualstudio.com/items?itemName=ms-vscode.makefile-tools): Suporte para projetos baseados em Makefile.
- [**Code Spell Checker**](https://marketplace.visualstudio.com/items?itemName=streetsidesoftware.code-spell-checker): Verificador ortográfico para melhorar a qualidade do código e documentação.

Você pode instalar essas extensões na Visual Studio Code Marketplace.

## Compilando o Projeto

Para compilar o projeto, execute o seguinte comando:

```bash
make
```

Isso gerará os seguintes arquivos no diretório `build/`:

- `emebedded_sys_2025.2.elf`: Arquivo executável
- `emebedded_sys_2025.2.hex`: Arquivo Intel HEX
- `emebedded_sys_2025.2.bin`: Arquivo binário

## Gravando o Firmware

Para gravar o firmware no microcontrolador STM32L476RG, conecte sua placa via ST-Link e execute:

```bash
make upload
```

Isso gravará o arquivo binário na memória flash do microcontrolador no endereço `0x8000000`.

## Limpando a Compilação

Para limpar os arquivos gerados na compilação, execute:

```bash
make clean
```

## Configuração do Projeto

O projeto está configurado para o microcontrolador STM32L476RG com as seguintes definições:

- **CPU**: Cortex-M4
- **FPU**: FPv4-SP-D16
- **Float ABI**: Hard
- **Otimização**: Debug (`-Og`)

Você pode modificar essas configurações no `Makefile`.

## Licença

Este projeto está licenciado sob a Licença MIT. Consulte o arquivo `LICENSE` para mais detalhes.

## Desenvolvimento

### Contribuindo
Para contribuir com este projeto:

1. **Crie um Novo Branch**: Use nomes descritivos como `feature/nome-da-funcionalidade`
2. **Atualize com STM32CubeMX**: Use o arquivo `.ioc` para configurações de hardware
3. **Mantenha Arquitetura Modular**: Novos periféricos devem ter drivers separados
4. **Teste Funcionalidades**: Verifique todos os estados da máquina de estados
5. **Documente Mudanças**: Atualize README e state_machine_diagram.md
6. **Envie Pull Request**: Mantenha o branch atualizado com `main`

### Debugging
- Use `HAL_UART_Transmit` para debug via serial
- Habilite `#define DEBUG` para mensagens de transição de estado
- Monitor o sistema através das mensagens de debug UART
- Utilize breakpoints nos callbacks para verificar fluxo
- Verifique timeouts de I2C/SPI para detectar problemas de hardware
- Use osciloscópio para analisar sinais I2C/SPI em caso de falhas

### Adicionando Novos Sensores
Para adicionar um novo sensor ao sistema:

1. **Defina o Canal ADC**: Escolha um dos 4 canais (AIN0-AIN3)
2. **Crie Ícone no Display**: Adicione definição em `main.c` (formato 8 bytes)
3. **Adicione Comando**: Modifique `process_uart_commands()` para reconhecer novo comando
4. **Configure Buffer**: Adicione ponteiro para ícone em `display_buffer[1]`
5. **Teste**: Verifique leitura ADC e exibição no display

Exemplo:
```c
// Em main.c
#define HUMIDITY_SCREEN { \
    0b00000000,           \
    0b00111100,           \
    // ... padrão 8x8
}
uint8_t humidity_screen[] = HUMIDITY_SCREEN;

// Em process_uart_commands()
else if (strncmp(cmd, "Humid", 5) == 0)
{
    display_buffer[1] = humidity_screen;
    channel_index = 2; // Usar AIN2
}
```

### Modificando Estados
Para adicionar novos estados à máquina:

1. **Atualize Enum**: Adicione novo estado em `typedef enum SystemState`
2. **Adicione Case**: Inclua novo case no `switch(get_system_state())`
3. **Configure Transições**: Atualize callbacks para transitar corretamente
4. **Documente**: Atualize diagrama de estados e documentação
5. **Teste Fluxo**: Verifique todas as transições possíveis

## Troubleshooting

### Problemas Comuns:

#### 1. I2C não responde
- Verifique conexões SDA/SCL e pull-ups (4.7kΩ recomendado)
- Confirme endereço I2C do PCF8591 (0x48 padrão)
- Verifique alimentação do PCF8591
- Use osciloscópio/analisador lógico para verificar sinais

#### 2. Valores ADC incorretos
- Confirme alimentação estável do PCF8591 (3.3V ou 5V)
- Verifique conexão do sensor ao canal correto (AIN0-AIN3)
- Confirme que sensor fornece tensão dentro da faixa (0-Vcc)
- Leia canal múltiplas vezes para descartar primeira leitura

#### 3. Display LED não acende
- Verifique conexões SPI (MOSI, SCK, CS)
- Confirme alimentação do MAX7219 (5V recomendado)
- Verifique CS (deve ser PA4 por padrão)
- Confirme matriz LED está corretamente conectada ao MAX7219
- Teste com comandos simples (ex: `Temp`)

#### 4. Display mostra padrão incorreto
- Verifique orientação da matriz LED
- Confirme ordem dos pinos do display
- Ajuste configuração de scan limit e decode mode
- Verifique definição dos ícones em `main.c`

#### 5. UART não funciona
- Verifique baudrate (115200 bps)
- Confirme configuração do terminal (8N1, sem flow control)
- Teste conexão TX/RX (podem estar invertidas)
- Verifique se driver USB-UART está instalado

#### 6. Comandos não reconhecidos
- Certifique-se de usar sintaxe correta:
  - `Read_AIN0` (não `read_ain0` ou `ReadAIN0`)
  - `Set_DAC_128` (não `SetDAC128` ou `Set_DAC 128`)
  - `Temp` (não `temp` ou `TEMP`)
- Sempre termine comandos com Enter (\n) ou Carriage Return (\r)
- Verifique buffer circular (1024 bytes) não está cheio

#### 7. Sistema travado
- Reset do microcontrolador (botão RESET)
- Verifique se há deadlock em operações I2C/SPI
- Confirme que callbacks estão sendo chamados
- Use modo DEBUG para ver transições de estado
- Verifique timeouts de I2C/SPI

#### 8. Display não atualiza periodicamente
- Confirme TIM2 está iniciado (`HAL_TIM_Base_Start_IT`)
- Verifique callback `HAL_TIM_PeriodElapsedCallback`
- Confirme sistema retorna para STATE_1 após operações
- Verifique se comando de monitoramento foi enviado (Temp/Volt/LDR)

#### 9. Tendência sempre mostra mesmo símbolo
- Verifique se sensor está realmente variando
- Confirme leitura anterior está sendo salva corretamente
- Verifique lógica de comparação em `PCF8591_RxCpltCallback`
- Teste com valores conhecidos mudando fisicamente o sensor

#### 10. Compilação falha
- Execute `make clean` antes de `make`
- Verifique se todos os arquivos .c/.h estão no Makefile
- Confirme toolchain ARM está instalado corretamente
- Verifique espaço em disco disponível

## Referências

- [Datasheet PCF8591](https://www.nxp.com/docs/en/data-sheet/PCF8591.pdf) - ADC/DAC 8-bit I2C
- [Datasheet MAX7219](https://datasheets.maximintegrated.com/en/ds/MAX7219-MAX7221.pdf) - LED Display Driver
- [STM32L476RG Reference Manual](https://www.st.com/resource/en/reference_manual/rm0351-stm32l47xxx-stm32l48xxx-stm32l49xxx-and-stm32l4axxx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf) - Microcontrolador
- [Documentação STM32 HAL](https://www.st.com/en/embedded-software/stm32cube.html) - Hardware Abstraction Layer
- [I2C Protocol Guide](https://www.ti.com/lit/an/slva704/slva704.pdf) - Especificação do protocolo I2C
- [SPI Protocol Guide](https://www.analog.com/en/analog-dialogue/articles/introduction-to-spi-interface.html) - Especificação do protocolo SPI
- [STM32L4 DMA Documentation](https://www.st.com/resource/en/application_note/dm00046011-using-the-stm32f2-stm32f4-and-stm32f7-series-dma-controller-stmicroelectronics.pdf) - Direct Memory Access