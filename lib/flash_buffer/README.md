
# Flash Buffer - Armazenamento Não Volátil

## Visão Geral

O módulo `flash_buffer` permite salvar amostras de sensores na memória flash do ESP32 de forma não volátil usando o sistema NVS (Non-Volatile Storage) da ESP-IDF.

## Características

- ✅ **Persistente**: Dados sobrevivem a resets e desligamentos
- ✅ **Buffer Circular**: Automaticamente sobrescreve dados antigos quando cheio
- ✅ **Wear Leveling**: Gerenciado automaticamente pela ESP-IDF
- ✅ **Fácil de usar**: API simples e intuitiva
- ✅ **Múltiplos buffers**: Suporta diferentes namespaces
- ✅ **Robusto**: Tratamento de dados corrompidos e validação de integridade
- ✅ **Seguro**: Inicialização com zeros e verificação de tamanhos

## Como Funciona

### NVS (Non-Volatile Storage)

O NVS é um sistema de armazenamento chave-valor da ESP-IDF que:
- Usa uma partição dedicada na flash (~24KB por padrão)
- Implementa wear leveling para prolongar vida útil da flash
- Gerencia automaticamente a escrita e leitura
- Suporta múltiplos namespaces para organização

### Buffer Circular

```
Índice:  0    1    2    3    4    ...   49
        [A1] [A2] [A3] [A4] [A5]  ...  [A50]
                                         ↑
                                    write_index

Quando cheio (50 amostras):
- Próxima escrita sobrescreve A1
- write_index volta para 0
- Sempre mantém as 50 amostras mais recentes
```

## Uso Básico

### 1. Inicialização

```c
#include "sensor_flash_buffer.h"

// Inicializa NVS (uma vez no app_main)
ESP_ERROR_CHECK(flash_buffer_system_init());

// Cria buffer com 50 amostras
flash_buffer_t* buffer = flash_buffer_init("sensors", 50);
```

### 2. Salvar Amostras

```c
sensor_read_t sample = {
    .temperature = 25.5,
    .humidity = 60.0,
    .lux = 450.0,
    .noise_level = 120
};

esp_err_t err = flash_buffer_write(buffer, &sample);
```

### 3. Ler Amostras

```c
// Ler última amostra
sensor_read_t last_sample;
if (flash_buffer_read_last(buffer, &last_sample)) {
    printf("Última: %.1f°C\n", last_sample.temperature);
}

// Ler múltiplas amostras (mais recentes primeiro)
sensor_read_t samples[10] = {0};
uint32_t count = flash_buffer_read(buffer, samples, 10);

for (uint32_t i = 0; i < count; i++) {
    printf("[%lu] Temp: %.1f°C\n", i, samples[i].temperature);
}
```

### 4. Consultar Estado

```c
// Número de amostras armazenadas
uint32_t count = flash_buffer_get_count(buffer);

// Verificar se está cheio
bool full = flash_buffer_is_full(buffer);

printf("Buffer: %lu/50 amostras%s\n", count, full ? " (cheio)" : "");
```

### 5. Limpar e Finalizar

```c
// Limpar todas as amostras
flash_buffer_clear(buffer);

// Fechar buffer (ao desligar)
flash_buffer_deinit(buffer);
```

## Integração com Sensor Monitor

```c
// Callback que salva na flash automaticamente
void save_to_flash_callback(sensor_base_t* sensor, void* data) {
    sensor_read_t sample = {0};
    
    // Preenche sample com dados do sensor
    if (strcmp(sensor->name, "DHT11") == 0) {
        dht11_data_t* dht_data = (dht11_data_t*)data;
        sample.temperature = dht_data->temperature;
        sample.humidity = dht_data->humidity;
    }
    
    // Salva na flash
    flash_buffer_write(global_flash_buffer, &sample);
}

// Cria monitor com callback
sensor_monitor_t* monitor = new_sensor_monitor(
    &dht11,
    10000,  // 10 segundos
    sizeof(dht11_data_t),
    "DHT11",
    save_to_flash_callback  // ← Salva automaticamente
);
```

## Recuperação Após Reset

```c
void app_main(void) {
    // Inicializa NVS (obrigatório uma vez)
    ESP_ERROR_CHECK(flash_buffer_system_init());
    
    // Cria buffer especificando tamanho de cada amostra
    flash_buffer_t* buffer = flash_buffer_init("sensors", sizeof(sensor_read_t), 50);
    
    if (!buffer) {
        ESP_LOGE("main", "Falha ao criar buffer");
        return;
    }
    
    // Automaticamente recupera amostras anteriores!
    uint32_t count = flash_buffer_get_count(buffer);
    
    if (count > 0) {
        printf("Recuperadas %lu amostras de execução anterior\n", count);
        
        // Recupera 10 amostras
        sensor_read_t samples[10] = {0};
        uint32_t read = flash_buffer_read(buffer, samples, 10);
        
        /*
        ... Processar amostras recuperadas ...
         */
    }
    
    // Continuar com inicialização dos sensores...
}
```

## Correções Recentes (v1.1)

### Problema de Dados Corrompidos - RESOLVIDO ✓

**Sintoma**: Valores absurdos ao ler da flash (ex: `-17826336093472827671582867456.0°C`)

**Causa**: Cálculo incorreto de offset de memória em `flash_buffer_read()` ao trabalhar com `void*`.

**Solução Implementada**:

1. **Correção do offset de memória**:
   ```c
   // ANTES (errado)
   nvs_get_blob(buffer->nvs_handle, key, &samples[i], &size);
   
   // DEPOIS (correto)
   uint8_t *sample_ptr = (uint8_t *)samples;
   void *dest = sample_ptr + (successfully_read * buffer->sample_size);
   nvs_get_blob(buffer->nvs_handle, key, dest, &size);
   ```

2. **Inicialização segura**:
   - Inicializa memória com zeros antes de cada leitura
   - Previne valores de lixo em caso de erro

3. **Tratamento robusto de erros**:
   - Pula amostras não encontradas (ESP_ERR_NVS_NOT_FOUND)
   - Verifica tamanho lido vs esperado
   - Continua lendo mesmo se algumas amostras falharem

4. **Validação de dados**:
   - Recomenda-se validar valores lidos (ranges razoáveis)
   - Exemplo: temperatura entre -50°C e 100°C

## Limitações e Considerações

### Tamanho e Performance

- **Máximo de amostras**: ~100 por namespace (limitação do NVS)
- **Tamanho da amostra**: Até 512 bytes (atualmente usa ~16 bytes)
- **Velocidade**: Escrita na flash é mais lenta (~10ms)
- **Durabilidade**: Flash suporta ~100.000 ciclos de escrita

### Recomendações

1. **Não salve a cada leitura se ler muito rápido** (< 1s)
   - Use para dados importantes ou amostras espaçadas (≥ 5s)
   - Exemplo: throttling de 10 segundos no callback

2. **Sempre inicialize arrays antes de ler**:
   ```c
   sensor_read_t samples[10] = {0};  // ← IMPORTANTE!
   flash_buffer_read(buffer, samples, 10);
   ```

3. **Use múltiplos namespaces para diferentes tipos**
   ```c
   flash_buffer_t* daily = flash_buffer_init("daily", sizeof(read_t), 100);
   flash_buffer_t* hourly = flash_buffer_init("hourly", sizeof(read_t), 50);
   ```

4. **Monitore o espaço disponível**
   - NVS tem ~24KB total
   - Cada namespace usa parte desse espaço

5. **Para dados críticos, considere verificação**
   ```c
   if (flash_buffer_write(buffer, &sample) != ESP_OK) {
       ESP_LOGE(TAG, "Falha ao salvar!");
       // Implementar retry ou alarme
   }
   ```

## Exemplo Completo

Veja `src/main_flash_example.c` para um exemplo completo integrando:
- Inicialização do NVS
- Criação do buffer
- Recuperação de dados após reset
- Salvamento automático com callbacks
- Monitoramento do estado do buffer

## Alternativas

Se precisar de características diferentes:

1. **SPIFFS/LittleFS**: Para arquivos grandes e estruturados
2. **FatFS (SD Card)**: Para muito mais espaço (MB/GB)
3. **RTC Memory**: Para poucos dados entre deep sleeps (<8KB)

## Troubleshooting

### Valores corrompidos/absurdos ao ler

**Sintomas**: 
- Temperaturas como `-17826336093472827671582867456.0°C`
- Valores NaN ou infinito
- Dados claramente incorretos

**Soluções**:
1. ✅ Sempre inicialize arrays: `sensor_read_t samples[10] = {0};`
2. ✅ Valide dados lidos (check de ranges)
3. ✅ Use versão atualizada do `flash_buffer` (v1.1+)
4. ✅ Limpe NVS se dados antigos estiverem corrompidos: `flash_buffer_clear(buffer)`

### Buffer não inicializa

**Possíveis causas**:
- `flash_buffer_system_init()` não foi chamado
- Namespace muito longo (máx 15 caracteres)
- Partição NVS cheia

**Solução**: 
```c
esp_err_t ret = flash_buffer_system_init();
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Erro NVS: %s", esp_err_to_name(ret));
    nvs_flash_erase();  // Último recurso
    flash_buffer_system_init();
}
```

### Leituras retornam 0 amostras

**Causas comuns**:
- Buffer vazio (primeira execução)
- Chaves NVS não encontradas (buffer foi limpo)
- Erro na leitura (check logs com nível DEBUG)

## Referências

- [ESP-IDF NVS Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)
- [ESP32 Flash Memory Layout](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/partition-tables.html)

---

**Última atualização**: Dezembro 2025 (v1.1)  
**Correções**: Problema de dados corrompidos resolvido  
**Compatibilidade**: ESP-IDF 5.5.0+
