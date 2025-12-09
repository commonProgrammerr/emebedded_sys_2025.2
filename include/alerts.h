#ifndef ALERTS_H
#define ALERTS_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>
#include <esp32-dht11.h> // Para o tipo dht11_t

// --- Configurações de Hardware ---
#define PIN_LED_GREEN   16
#define PIN_LED_YELLOW  17
#define PIN_LED_RED     5
#define PIN_BUZZER      18

// --- Limites do Biotério (Spec MVP) ---
// Temperatura: 22-26°C
#define TEMP_MIN 22.0
#define TEMP_MAX 26.0
// Umidade: 40-60%
#define HUM_MIN  40.0
#define HUM_MAX  60.0

// Luz: Dia 150-300 lux, Noite ~0
#define LUX_DAY_MIN 150
#define LUX_NIGHT_MAX 5 // Tolerância para vazamento de luz

// Ruído (Score 0-100)
#define NOISE_PEAK_LIMIT 60
#define NOISE_AVG_LIMIT  40

// --- Filas Globais (Separadas) ---
extern QueueHandle_t xQueueDHT;   // Carrega dht11_t
extern QueueHandle_t xQueueLight; // Carrega float
extern QueueHandle_t xQueueNoise; // Carrega uint16_t (score)

// --- Funções ---
void alerts_init();
void alerts_trigger_snooze(); // Chama quando apertar botão para silenciar
void alerts_set_night_mode(bool is_night); // Define se é horário de escuro (fotoperíodo)

#endif