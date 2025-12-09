#ifndef BH1750_H
#define BH1750_H

#include <stdint.h>
#include "driver/i2c_master.h"

// Endereços I2C do bh1750
#define BH1750_I2C_ADDR_LOW 0x23  // ADDR pino em LOW (padrão do GY-30)
#define BH1750_I2C_ADDR_HIGH 0x5C // ADDR pino em HIGH

// Comandos de instrução
#define BH1750_POWER_DOWN 0x00
#define BH1750_POWER_ON 0x01
#define BH1750_RESET 0x07
#define BH1750_CONT_H_RES 0x10  // Modo contínuo alta resolução 1lx
#define BH1750_CONT_H_RES2 0x11 // Modo contínuo alta resolução 0.5lx
#define BH1750_CONT_L_RES 0x13  // Modo contínuo baixa resolução 4lx
#define BH1750_ONE_H_RES 0x20   // Modo única leitura alta resolução 1lx
#define BH1750_ONE_H_RES2 0x21  // Modo única leitura alta resolução 0.5lx
#define BH1750_ONE_L_RES 0x23   // Modo única leitura baixa resolução 4lx

// Valor padrão do MTreg (Measurement Time Register)
#define BH1750_DEFAULT_MTREG 69

// Estrutura do sensor
typedef struct
{
  i2c_master_dev_handle_t dev_handle; // Handle do dispositivo I2C
  uint8_t address;                    // Endereço I2C
  uint8_t mtreg;                      // Valor do MTreg
  uint8_t mode;                       // Modo atual de medição
} bh1750_t;

/**
 * @brief Inicializa o sensor bh1750.
 *
 * @param i2c_bus_handle Handle do barramento I2C mestre.
 * @param address Endereço I2C do sensor.
 * @param bh1750 Ponteiro para a estrutura do sensor.
 * @return esp_err_t ESP_OK em caso de sucesso.
 */
esp_err_t bh1750_init(i2c_master_bus_handle_t i2c_bus_handle, uint8_t address, bh1750_t *bh1750);

/**
 * @brief Configura o modo de medição do sensor.
 *
 * @param bh1750 Ponteiro para a estrutura do sensor.
 * @param mode Modo de medição (ex: BH1750_CONT_H_RES).
 * @param wait_ms Tempo de espera após configurar (0 para usar tempo padrão).
 * @return esp_err_t ESP_OK em caso de sucesso.
 */
esp_err_t bh1750_set_measurement_mode(bh1750_t *bh1750, uint8_t mode, uint32_t wait_ms);

/**
 * @brief Lê a iluminância em lux.
 *
 * @param bh1750 Ponteiro para a estrutura do sensor.
 * @param lux Ponteiro para armazenar o valor da iluminância.
 * @return esp_err_t ESP_OK em caso de sucesso.
 */
esp_err_t bh1750_read_lux(bh1750_t *bh1750, float *lux);

/**
 * @brief Configura o valor do MTreg (Measurement Time Register).
 *        Isso ajusta a sensibilidade do sensor.
 *
 * @param bh1750 Ponteiro para a estrutura do sensor.
 * @param mtreg Valor do MTreg (31 a 254).
 * @return esp_err_t ESP_OK em caso de sucesso.
 */
esp_err_t bh1750_set_mtreg(bh1750_t *bh1750, uint8_t mtreg);

/**
 * @brief Força um reset no sensor.
 *
 * @param bh1750 Ponteiro para a estrutura do sensor.
 * @return esp_err_t ESP_OK em caso de sucesso.
 */
esp_err_t bh1750_reset(bh1750_t *bh1750);

/**
 * @brief Coloca o sensor em modo de baixo consumo.
 *
 * @param bh1750 Ponteiro para a estrutura do sensor.
 * @return esp_err_t ESP_OK em caso de sucesso.
 */
esp_err_t bh1750_power_down(bh1750_t *bh1750);

/**
 * @brief Ativa o sensor após power down.
 *
 * @param bh1750 Ponteiro para a estrutura do sensor.
 * @return esp_err_t ESP_OK em caso de sucesso.
 */
esp_err_t bh1750_power_on(bh1750_t *bh1750);

/**
 * @brief Desinicializa o sensor (libera recursos).
 *
 * @param bh1750 Ponteiro para a estrutura do sensor.
 * @return esp_err_t ESP_OK em caso de sucesso.
 */
esp_err_t bh1750_deinit(bh1750_t *bh1750);

#endif // BH1750_H