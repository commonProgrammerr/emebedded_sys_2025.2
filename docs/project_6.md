# Documentação de Implementação do Projeto

## Visão Geral

Este documento descreve detalhadamente a implementação do código do projeto embarcado para STM32L476, focando na lógica de controle desenvolvida customizadamente. O sistema implementa um controle multi-modal com leitura de sensores via I2C (PCF8591), geração de PWM, geração de onda senoidal via DAC, e debouncing de botão.

## Configurações do CubeMX

O projeto utiliza os seguintes periféricos configurados via STM32CubeMX:

- **ADC3**: Conversor analógico-digital de 12 bits (não utilizado na implementação atual)
- **DAC1**: Conversor digital-analógico (canal 2) para geração de forma de onda
- **I2C3**: Interface I2C para comunicação com PCF8591 (ADC/DAC externo)
- **TIM1**: Timer para geração de PWM (canal 4)
- **TIM2**: Timer de 10 segundos para leituras periódicas
- **TIM3**: Timer de 1ms para debouncing de botão
- **TIM4**: Timer para trigger do DAC via DMA
- **USART2**: Comunicação serial para debug (115200 baud)
- **DMA**: Configurado para I2C, DAC e USART
- **GPIO**: Botão B1 com interrupção externa, pinos de controle I2C (VCC/GND)

## Arquitetura do Sistema

### Máquina de Estados

O sistema é baseado em uma máquina de estados com 7 estados principais:

```
SYSTEM_STATE_1: Estado ocioso - aguardando eventos
SYSTEM_STATE_2: Configuração do canal ADC via I2C
SYSTEM_STATE_3: Leitura do canal ADC via I2C
SYSTEM_STATE_4: Processamento e atualização do duty cycle PWM
SYSTEM_STATE_5: Configuração do canal ADC para controle do DAC
SYSTEM_STATE_6: Leitura do ADC para controle do DAC
SYSTEM_STATE_7: Atualização da amplitude da onda DAC
```

### Modos de Operação

O sistema opera em 3 modos distintos, alternados pelo botão B1:

**Modo 1**: Controle PWM apenas
- Lê LDR (sensor de luz) no canal A3 do PCF8591
- Ajusta duty cycle do PWM baseado na leitura do sensor
- DAC desabilitado

**Modo 2**: Geração de onda senoidal apenas
- Gera onda senoidal via DAC usando DMA
- Lê potenciômetro no canal A0 do PCF8591
- Ajusta amplitude da onda senoidal baseado no potenciômetro
- PWM desabilitado

**Modo 3**: PWM + Onda senoidal simultâneos
- Combina funcionalidades dos modos 1 e 2
- Lê LDR para PWM
- Lê potenciômetro para amplitude da onda

## Implementação Detalhada

### 1. Proteção de Concorrência (Thread Safety)

O código implementa macros para acesso atômico a variáveis compartilhadas entre ISRs e main loop:

```c
#define ATOMIC_READ(var) ({ \
  __disable_irq();          \
  typeof(var) tmp = (var);  \
  __enable_irq();           \
  tmp;                      \
})

#define ATOMIC_WRITE(var, value) \
  do                             \
  {                              \
    __disable_irq();             \
    (var) = (value);             \
    __enable_irq();              \
  } while (0)
```

**Justificativa**: Garante que leituras e escritas em variáveis voláteis sejam atômicas, prevenindo race conditions entre interrupções e código principal.

### 2. Variáveis Voláteis Globais

```c
volatile SystemState system_state = SYSTEM_STATE_1;
volatile i2c_flag_t i2c = idle;
volatile uint8_t mode = 1;
volatile uint8_t read = 0;
volatile uint8_t button_pressed = 0;
volatile uint8_t debouncing = 0;
volatile uint16_t debounce_counter = 0;
```

**Uso de `volatile`**: Necessário pois estas variáveis são modificadas tanto em ISRs quanto no main loop, garantindo que o compilador não otimize acessos inadequadamente.

### 3. Estrutura de Configuração do PCF8591

```c
typedef struct pcf8591_config
{
  uint8_t : 1;
  uint8_t analog_output_enabled : 1;
  analog_input_programming_t analog_input_programming : 2;
  uint8_t : 1;
  uint8_t auto_increment_flag : 1;
  pcf8591_channel_t adc_selected_channel : 2;
} pcf8591_config_t;
```

**Implementação**: Usa bitfields para mapear exatamente o byte de controle do PCF8591, permitindo configuração direta do CI via I2C.

**Configuração padrão**:
- Saída analógica habilitada
- 4 canais single-ended
- Auto-incremento desabilitado
- Canal inicial: A0

### 4. Debouncing de Botão

O sistema implementa debouncing por timer (TIM3 configurado para 1ms):

```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == B1_Pin && !debouncing)
  {
    debouncing = 1;
    debounce_counter = 0;
    HAL_TIM_Base_Start_IT(&htim3);
  }
}
```

**Fluxo do debouncing**:
1. Detecção de borda de descida no botão (GPIO_EXTI_Callback)
2. Inicia timer de 1ms se não estiver em debounce
3. Timer incrementa contador a cada 1ms
4. Após 50ms (DEBOUNCE_TIME_MS), verifica estado do botão
5. Se botão ainda pressionado, confirma pressionamento válido
6. Define flag `button_pressed` para processamento no main loop

**Vantagem**: Elimina ruído mecânico do botão sem bloquear o sistema, usando timer por interrupção.

### 5. Geração de Onda Senoidal

A função `populate_sin_wave_buff()` implementa dois métodos de atualização:

#### Método 1: Atualização Proporcional (update = 1)
```c
double ratio = (double)wave_amplitude / amplitude;
for (size_t i = 0; i < SIN_WAVE_SAMPLES; ++i)
  sin_wave_buff[i] = (uint16_t)(sin_wave_buff[i] * ratio);
```

**Vantagem**: Eficiente - apenas multiplica valores existentes por uma razão.

**Uso**: Quando buffer já contém onda e precisa apenas ajustar amplitude.

#### Método 2: Recálculo Completo (update = 0)
```c
for (size_t i = 0; i < SIN_WAVE_SAMPLES; ++i)
{
  double v = (sin(2.0 * M_PI * i / SIN_WAVE_SAMPLES) + 1.0) / 2.0;
  sin_wave_buff[i] = (uint16_t)(v * wave_amplitude);
}
```

**Uso**: Inicialização ou quando amplitude muito baixa (< 256), evitando perda de precisão.

**Características**:
- 512 amostras por ciclo
- Amplitude ajustável de 0 a 4095 (12 bits)
- Protegido por desabilitar IRQs durante atualização

### 6. Funções de Comunicação I2C com PCF8591

#### Configuração de Canal
```c
HAL_StatusTypeDef PCF8591_set_channel_index(pcf8591_channel_t channel_index)
{
  if (channel_index > 3)
    return HAL_ERROR;
  
  pcf8591_config.adc_selected_channel = channel_index;
  return HAL_I2C_Master_Transmit_DMA(&I2C_INTERFACE, PCF8591_ADDRESS, 
                                      (uint8_t *)&pcf8591_config, 1);
}
```

**Implementação**: Envia byte de configuração via I2C DMA para selecionar canal ADC.

#### Leitura de Canal
```c
HAL_StatusTypeDef PCF8591_read_analog_channel()
{
  return HAL_I2C_Master_Receive_DMA(&I2C_INTERFACE, PCF8591_ADDRESS, 
                                     (uint8_t *)&pcf8591_value, 2);
}
```

**Detalhe importante**: Lê 2 bytes, pois primeiro byte do PCF8591 é sempre o valor anterior (dummy byte).

### 7. Callbacks de I2C

```c
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c) { i2c = tx; }
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) { i2c = rx; }
```

**Função**: Sinaliza conclusão de operações I2C, permitindo progressão da máquina de estados.

### 8. Máquina de Estados - Função `next_state()`

Implementa transições de estado baseadas em flags:

```c
void next_state()
{
  switch (get_system_state())
  {
  case SYSTEM_STATE_1:
    if (ATOMIC_READ(read))
    {
      uint8_t mod = ATOMIC_READ(mode);
      if (mod == 1 || mod == 3)
        set_system_state(SYSTEM_STATE_2);
      else if (mod == 2)
        set_system_state(SYSTEM_STATE_5);
    }
    break;
  // ... outras transições
  }
}
```

**Lógica de transição**:
- Estados 1→2: Timer disparou leitura, modo 1 ou 3 (PWM ativo)
- Estados 2→3: I2C TX completo
- Estados 3→4: I2C RX completo
- Estados 4→1: Retorna ao idle (modo 1)
- Estados 4→5: Vai para leitura DAC (modo 3)
- Estados 5→6→7→1: Fluxo similar para controle DAC

### 9. Alternância de Modos - Função `switch_mode()`

```c
void switch_mode()
{
  ATOMIC_WRITE(mode, (mode % 3) + 1);
  switch (ATOMIC_READ(mode))
  {
  case 1:
    HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_2);
    HAL_DAC_Stop(&hdac1, DAC_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
    break;
  case 2:
    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_4);
    HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_2, (uint32_t *)sin_wave_buff, 
                      SIN_WAVE_SAMPLES, DAC_ALIGN_12B_R);
    break;
  case 3:
    HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_2, (uint32_t *)sin_wave_buff, 
                      SIN_WAVE_SAMPLES, DAC_ALIGN_12B_R);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
    break;
  }
}
```

**Gerenciamento de recursos**:
- Para recursos não utilizados no modo atual
- Inicia apenas recursos necessários
- Evita conflitos e economiza energia

### 10. Callback de Timer - `HAL_TIM_PeriodElapsedCallback()`

Gerencia dois timers diferentes:

#### TIM2 (Período de 10 segundos)
```c
if (htim->Instance == TIM2 && get_system_state() == SYSTEM_STATE_1)
{
  read = 1;
}
```
**Função**: Dispara leituras periódicas quando em estado idle.

#### TIM3 (Período de 1ms para debouncing)
```c
else if (htim->Instance == TIM3)
{
  debounce_counter++;
  if (debounce_counter >= DEBOUNCE_TIME_MS)
  {
    if (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET)
    {
      button_pressed = 1;
    }
    HAL_TIM_Base_Stop_IT(&htim3);
    debouncing = 0;
    debounce_counter = 0;
  }
}
```
**Função**: Implementa debouncing por tempo.

### 11. Loop Principal - `main()`

#### Inicialização
```c
set_system_state(SYSTEM_STATE_1);
HAL_TIM_Base_Start_IT(&htim2);
HAL_TIM_Base_Start(&htim4);
HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
populate_sin_wave_buff(SIN_WAVE_MAX_AMPLITUDE, 0);
```

#### Loop Infinito com Switch-Case por Estado

**SYSTEM_STATE_1 (Idle)**:
```c
__WFI(); // Wait For Interrupt - economiza energia
if (ATOMIC_READ(button_pressed))
{
  ATOMIC_WRITE(button_pressed, 0);
  switch_mode();
}
```
**Otimização**: Usa WFI para modo de baixo consumo enquanto aguarda eventos.

**SYSTEM_STATE_2 (Config ADC para PWM)**:
```c
ATOMIC_WRITE(read, 0);
PCF8591_set_channel_index(PCF8591_CHANNEL_A3); // LDR
__WFI();
```
**Função**: Configura canal A3 (LDR) para leitura.

**SYSTEM_STATE_3 (Leitura ADC para PWM)**:
```c
PCF8591_read_analog_channel();
__WFI();
```

**SYSTEM_STATE_4 (Atualização PWM)**:
```c
pcf8591_value &= 0x00FF; // Extrai apenas lower byte
set_pwm_duty(&htim1, TIM_CHANNEL_4, pcf8591_value);
ATOMIC_WRITE(i2c, idle);
```
**Detalhe**: Valor do PCF8591 (0-255) é diretamente usado como duty cycle (0-255).

**SYSTEM_STATE_5 (Config ADC para DAC)**:
```c
ATOMIC_WRITE(read, 0);
PCF8591_set_channel_index(PCF8591_CHANNEL_A0); // Potenciômetro
__WFI();
```

**SYSTEM_STATE_6 (Leitura ADC para DAC)**:
```c
PCF8591_read_analog_channel();
__WFI();
```

**SYSTEM_STATE_7 (Atualização Amplitude DAC)**:
```c
pcf8591_value &= 0x00FF;
uint16_t new_wave_amplitude = (uint16_t)((UINT8_MAX - pcf8591_value) * 
                                          SIN_WAVE_MAX_AMPLITUDE / UINT8_MAX);
populate_sin_wave_buff(new_wave_amplitude, 1);
ATOMIC_WRITE(i2c, idle);
```
**Cálculo**: Inverte valor do potenciômetro (255-valor) e escala para 0-4095 (12 bits).

**Após cada estado**:
```c
next_state(); // Avança máquina de estados
```

### 12. Função de Controle PWM

```c
static void set_pwm_duty(TIM_HandleTypeDef *htim, uint32_t channel, uint8_t duty)
{
  __HAL_TIM_SET_COMPARE(htim, channel, duty);
}
```

**Implementação**: Wrapper simples para modificar registro de comparação do timer.

**Range**: 0-255 (conforme configuração TIM1.Period = 255).

### 13. Funções de Acesso Seguro ao Estado

```c
static void set_system_state(SystemState new_state)
{
#ifdef DEBUG
  if (new_state != system_state)
  {
    static char msg[50];
    snprintf(msg, sizeof(msg), "[debug] State changed from %d to %d\r\n", 
             system_state, new_state);
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
  }
#endif
  ATOMIC_WRITE(system_state, new_state);
}

static inline SystemState get_system_state(void)
{
  return ATOMIC_READ(system_state);
}
```

**Recursos**:
- Debug logging condicional (apenas quando `DEBUG` definido)
- Acesso atômico garantido
- Inlining da função de leitura para performance

## Fluxos de Execução

### Fluxo 1: Modo 1 - Controle PWM

```
1. Timer TIM2 dispara a cada 10s → read = 1
2. Estado 1 → Estado 2: Configura canal A3 (LDR)
3. I2C TX completo → Estado 2 → Estado 3
4. Estado 3: Inicia leitura I2C
5. I2C RX completo → Estado 3 → Estado 4
6. Estado 4: Atualiza PWM duty cycle
7. Estado 4 → Estado 1: Retorna ao idle
8. Aguarda próximo timer ou botão
```

### Fluxo 2: Modo 2 - Geração de Onda Senoidal

```
1. DAC rodando continuamente via DMA
2. Timer TIM2 dispara a cada 10s → read = 1
3. Estado 1 → Estado 5: Configura canal A0 (POT)
4. I2C TX completo → Estado 5 → Estado 6
5. Estado 6: Inicia leitura I2C
6. I2C RX completo → Estado 6 → Estado 7
7. Estado 7: Atualiza amplitude da onda senoidal
8. Estado 7 → Estado 1: Retorna ao idle
9. Aguarda próximo timer ou botão
```

### Fluxo 3: Modo 3 - PWM + DAC Simultâneos

```
1. DAC e PWM rodando simultaneamente
2. Timer TIM2 dispara → read = 1
3. Estado 1 → Estado 2: Lê LDR para PWM
4. Estados 2 → 3 → 4: Atualiza PWM
5. Estado 4 → Estado 5: Lê POT para DAC
6. Estados 5 → 6 → 7: Atualiza amplitude DAC
7. Estado 7 → Estado 1: Retorna ao idle
8. Ciclo se repete
```

### Fluxo 4: Pressionamento de Botão (qualquer modo)

```
1. Botão pressionado → GPIO_EXTI IRQ
2. Inicia TIM3 (debounce timer)
3. TIM3 dispara a cada 1ms por 50ms
4. Após 50ms: verifica se botão ainda pressionado
5. Se sim: button_pressed = 1
6. Main loop detecta flag
7. Chama switch_mode()
8. Para/inicia recursos conforme novo modo
9. Retorna ao estado idle
```

## Otimizações Implementadas

### 1. Uso de `__WFI()` (Wait For Interrupt)
- Coloca CPU em modo de baixo consumo
- Desperta apenas com interrupções
- Economiza energia em sistema battery-powered

### 2. Atualização Incremental de Onda Senoidal
- Evita recálculo de seno quando possível
- Usa razão de amplitudes para ajuste rápido
- Recalcula apenas quando necessário (amplitude muito baixa)

### 3. DMA para I2C, DAC e UART
- Libera CPU de operações de transferência
- Permite processamento paralelo
- Callbacks sinalizam conclusão de operações

### 4. Acesso Atômico com IRQ Disable
- Minimiza tempo com interrupções desabilitadas
- Usa macros para clareza e consistência
- Previne race conditions

### 5. Inline de Funções Críticas
- `get_system_state()` marcada como inline
- Elimina overhead de chamada de função
- Melhora performance em hot path

## Considerações de Debug

O código inclui suporte a debug condicional via `#ifdef DEBUG`:

```c
#ifdef DEBUG
  char tx[50];
  snprintf(tx, sizeof(tx), "[debug] pcf8591=%u\r\n", pcf8591_value);
  HAL_UART_Transmit(&huart2, (uint8_t *)tx, strlen(tx), HAL_MAX_DELAY);
#endif
```

**Mensagens de debug disponíveis**:
- Transições de estado
- Valores lidos do PCF8591
- Amplitudes calculadas para onda senoidal

**Ativação**: Definir `#define DEBUG` antes da compilação.

## Limitações e Possíveis Melhorias

### Limitações Atuais
1. **ADC3 não utilizado**: Configurado mas não implementado
2. **Tratamento de erros I2C limitado**: TODO comments indicam necessidade
3. **Frequência fixa da onda senoidal**: Determinada por TIM4
4. **Valores hardcoded**: Canais PCF8591 fixos no código

### Melhorias Sugeridas
1. Implementar tratamento robusto de erros HAL
2. Adicionar timeout para operações I2C
3. Permitir ajuste de frequência da onda
4. Criar menu de configuração via UART
5. Implementar salvamento de configurações em Flash
6. Adicionar modo sleep mais agressivo
7. Implementar watchdog para robustez

## Conclusão

A implementação demonstra uso avançado de recursos do STM32:
- Máquina de estados robusta
- Sincronização entre ISRs e main loop
- Uso eficiente de DMA
- Otimização de energia com WFI
- Debouncing por timer
- Geração de formas de onda complexas
- Comunicação I2C assíncrona

O código é modular, bem estruturado e preparado para expansões futuras mantendo a robustez do sistema embarcado.
