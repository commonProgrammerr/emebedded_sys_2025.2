# Sistema Embarcado - Projeto I2C com PCF8591

Este projeto implementa uma interface de comunicação I2C com o módulo ADC/DAC PCF8591 utilizando o microcontrolador STM32L476RG. O sistema permite ler valores analógicos dos 4 canais de entrada (AIN0-AIN3) e controlar a saída analógica (DAC) através de comandos UART.

## Funcionalidades

- **Processamento de Comandos Baseado em Strings**: Sistema robusto de comandos textuais para melhor usabilidade
- **Leitura de Canais Analógicos**: Lê valores dos canais AIN0, AIN1, AIN2 e AIN3 do PCF8591
- **Controle de DAC**: Define valores de saída analógica (0-255) no PCF8591
- **Interface UART**: Comunicação serial para envio de comandos e recebimento de dados
- **Controle de Fluxo**: Implementa lógica de estados para operações I2C e UART
- **Comunicação I2C**: Interface com múltiplos dispositivos I2C (I2C1, I2C2, I2C3)
- **Debug Avançado**: Saídas de debug com prefixo `[debug]` para monitoramento do sistema

## Comandos Disponíveis

O sistema agora utiliza **comandos baseados em strings** para melhor clareza e funcionalidade expandida.

### Leitura de Canais ADC
- **`Read_AIN0`**: Lê o canal AIN0
- **`Read_AIN1`**: Lê o canal AIN1  
- **`Read_AIN2`**: Lê o canal AIN2
- **`Read_AIN3`**: Lê o canal AIN3

### Controle do DAC
- **`Set_DAC_<VALOR>`**: Define valor do DAC (0-255)
  - Exemplo: `Set_DAC_128` para valor 128
  - Exemplo: `Set_DAC_255` para valor máximo

## Arquitetura do Sistema

O sistema implementa controle de fluxo para gerenciar a comunicação I2C e processamento de comandos UART:

### Diagrama Geral do Sistema
![Diagrama de Estados do Sistema](docs/images/Diagarama%20de%20estados%20sistema.svg)

### Processamento de Comandos
![Diagrama de Estados dos Comandos](docs/images/Diagrama%20de%20estados%20cmd.svg)

*Documentação detalhada disponível em [docs/state_machine_diagram.md](docs/state_machine_diagram.md)*

### Descrição dos Diagramas:

**Diagrama do Sistema**: Mostra o fluxo de controle que gerencia as operações de I2C e UART do sistema.

**Diagrama de Comandos**: Detalha como o sistema processa os comandos baseados em strings recebidos via UART.

### Principais Funcionalidades:
- **Processamento de Comandos**: Sistema robusto de parsing de strings UART
- **Comunicação I2C**: Interface otimizada com PCF8591 para ADC/DAC
- **Estados de Controle**: Gestão eficiente de fluxo de dados
- **Validação de Comandos**: Verificação automática de sintaxe e parâmetros

### Fluxos de Operação:
- **Leitura ADC**: Comando `Read_AIN<N>` → Configuração I2C → Leitura de dados → Resposta UART
- **Configuração DAC**: Comando `Set_DAC_<VALOR>` → Configuração I2C → Confirmação UART
- **Processamento de Comandos**: Recepção UART → Parsing de strings → Validação → Execução

## Hardware Requerido

- **Microcontrolador**: STM32L476RG (Nucleo-L476RG)
- **Módulo ADC/DAC**: PCF8591 (endereço I2C: 0x48)
- **Conexões I2C**: SDA e SCL conectados ao I2C1 do STM32
- **UART**: USART2 para comunicação serial (115200 baud)

## Estrutura do Projeto

```
.
├── Core/
│   ├── Inc/                  # Arquivos de cabeçalho
│   │   ├── main.h           # Definições principais
│   │   ├── stm32l4xx_hal_conf.h
│   │   └── stm32l4xx_it.h   # Tratadores de interrupção
│   └── Src/                 # Código-fonte
│       ├── main.c           # Programa principal
│       ├── stm32l4xx_hal_msp.c
│       ├── stm32l4xx_it.c   # Implementação das interrupções
│       └── system_stm32l4xx.c
├── Drivers/                 # Drivers HAL e CMSIS
├── build/                   # Diretório de saída da compilação
├── docs/                    # Documentação do projeto
├── Makefile                 # Sistema de build
├── STM32L476XX_FLASH.ld     # Script de linker
├── startup_stm32l476xx.s    # Arquivo de inicialização
└── emebedded_sys_2025.2.ioc # Configuração do STM32CubeMX
```

## Como Usar

### 1. Configuração Inicial
Após compilar e gravar o firmware:

1. Conecte o módulo PCF8591 ao STM32L476RG via I2C1
2. Abra um terminal serial (115200 baud, 8N1)
3. O sistema exibirá: "Enter command (0-3 for AIN, w for DAC):"

### 2. Lendo Canais ADC
Para ler um canal analógico, envie o comando completo seguido de Enter:
```
> Read_AIN0
AIN0: 128

> Read_AIN2
AIN2: 255
```

### 3. Controlando o DAC
Para definir o valor do DAC, use o comando `Set_DAC_` seguido do valor:
```
> Set_DAC_128
Valor do DAC: 128

> Set_DAC_255
Valor do DAC: 255
```

### 4. Monitoramento do Sistema
O sistema exibe informações de debug em tempo real com prefixo `[debug]`:
```
[debug] Current state: 1
Enter command (0-3 for AIN, w for DAC):
[debug] Current state: 2
[debug] Current state: 3
[debug] Current state: 4
AIN0: 145
[debug] Current state: 1
```

## Configuração do PCF8591

- **Endereço I2C**: 0x48 (padrão)
- **Canais ADC**: A0, A1, A2, A3 (8-bit, 0-255)
- **DAC**: Saída analógica de 8-bit (0-255)
- **Alimentação**: 3.3V ou 5V

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
3. **Teste Funcionalidades**: Verifique o processamento de comandos e comunicação I2C/UART
4. **Envie Pull Request**: Mantenha o branch atualizado com `main`

### Debugging
- Use `HAL_UART_Transmit` para debug via serial
- Monitor o sistema através das mensagens de debug UART
- Verifique timeouts de I2C para detectar problemas de hardware

## Troubleshooting

### Problemas Comuns:
1. **I2C não responde**: Verifique conexões SDA/SCL e pull-ups
2. **Valores ADC incorretos**: Confirme alimentação do PCF8591
3. **UART não funciona**: Verifique baudrate e configuração do terminal
4. **Sistema travado**: Reset do sistema ou verificar timeouts de I2C
5. **Comandos não reconhecidos**: Certifique-se de usar a sintaxe correta (`Read_AIN0` ou `Set_DAC_128`)
6. **Comandos incompletos**: Sempre termine comandos com Enter (\n) ou Carriage Return (\r)

## Referências

- [Datasheet PCF8591](https://www.nxp.com/docs/en/data-sheet/PCF8591.pdf)
- [STM32L476RG Reference Manual](https://www.st.com/resource/en/reference_manual/rm0351-stm32l47xxx-stm32l48xxx-stm32l49xxx-and-stm32l4axxx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [Documentação STM32 HAL](https://www.st.com/en/embedded-software/stm32cube.html)
- [I2C Protocol Guide](https://www.ti.com/lit/an/slva704/slva704.pdf)