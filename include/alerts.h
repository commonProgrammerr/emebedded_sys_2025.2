#ifndef ALERTS_H
#define ALERTS_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>
#include <esp32-dht11.h> // Para o tipo dht11_t

// --- Filas Globais (Separadas) ---
extern QueueHandle_t xQueueDHT;   // Carrega dht11_t
extern QueueHandle_t xQueueLight; // Carrega float
extern QueueHandle_t xQueueNoise; // Carrega uint16_t (score)

// --- Funções ---
void alerts_init();
void alerts_trigger_snooze(); // Chama quando apertar botão para silenciar
void alerts_set_night_mode(bool is_night); // Define se é horário de escuro (fotoperíodo)

#endif