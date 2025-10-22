# Diagramas de Controle do Sistema

O projeto implementa sistemas de controle para gerenciar a comunicação I2C com o módulo PCF8591 ADC/DAC e processamento de comandos UART do usuário.

## Diagramas Visuais

### Diagrama Geral do Sistema
![Diagrama de Estados do Sistema](images/Diagarama%20de%20estados%20sistema.svg)

### Processamento de Comandos
![Diagrama de Estados dos Comandos](images/Diagrama%20de%20estados%20cmd.svg)

## Descrição dos Sistemas:

### Sistema de Processamento de Comandos
- **Buffer de Recepção**: Coleta caracteres UART até terminador (\n ou \r)
- **Parser de Comandos**: Analisa strings para identificar `Read_AIN<N>` ou `Set_DAC_<VALOR>`
- **Validação**: Verifica formato e parâmetros dos comandos
- **Execução**: Dispara operações I2C apropriadas

### Sistema de Comunicação I2C
- **Configuração ADC**: Seleciona canal analógico do PCF8591
- **Leitura de Dados**: Recebe valores de 8-bit dos canais ADC
- **Configuração DAC**: Define saída analógica no PCF8591
- **Gestão de Callbacks**: Processa conclusões de transmissão/recepção

### Sistema de Interface UART
- **Recepção de Comandos**: Processamento assíncrono de strings
- **Transmissão de Respostas**: Envio formatado de resultados
- **Debug**: Mensagens com prefixo `[debug]` para monitoramento

## Fluxos de Operação:

### Leitura de Canal ADC:
```
Comando UART → Parsing → Configuração I2C → Leitura I2C → Resposta UART
```

### Configuração de DAC:
```
Comando UART → Parsing → Validação → Configuração I2C → Confirmação UART
```

## Principais Funções:

- **`process_uart_commands()`**: Processa comandos baseados em strings
- **`PCF8591_UpdateAnalogChannelData()`**: Lê canais ADC via I2C
- **`PCF8591_SetAnalogOutput()`**: Configura DAC via I2C
- **Callbacks HAL**: Gerenciam conclusões de operações assíncronas