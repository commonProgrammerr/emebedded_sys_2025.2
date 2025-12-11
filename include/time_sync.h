#ifndef TIME_SYNC_H
#define TIME_SYNC_H
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include "esp_err.h"
#include <stdlib.h>
#include <time.h>

extern time_t now;
extern char strftime_buf[64];
extern struct tm timeinfo;

/**
 *  @brief Connect to WiFi network
 *  @param ssid - SSID of the WiFi network
 *  @param password - Password of the WiFi network
 *  @return esp_err_t - ESP_OK on success, ESP_FAIL or ESP_ERR_TIMEOUT otherwise
 */
esp_err_t connect_wifi(const char* ssid, const char* password);

/**
 *  @brief Shutdown WiFi connection and cleanup resources
 */
void shutdown_wifi(void);

/**
 *  @brief Initialize SNTP time synchronization (requires active WiFi connection)
 *  @param timezone - Timezone string (e.g., "BRT3" for Brazil Standard Time)
 *  @param ntp_server - NTP server address (e.g., "a.st1.ntp.br")
 *  @return esp_err_t - ESP_OK on success, ESP_ERR_TIMEOUT or other error code otherwise
 */
esp_err_t init_sntp_sync(const char* timezone, const char* ntp_server);

/**
 *  @brief Connect to WiFi and synchronize time with NTP server (all-in-one function)
 *  @param ssid - SSID of the WiFi network
 *  @param password - Password of the WiFi network
 *  @param timezone - Timezone string (e.g., "BRT3" for Brazil Standard Time)
 *  @param ntp_server - NTP server address (e.g., "a.st1.ntp.br")
 *  @return esp_err_t - ESP_OK on success, error code otherwise
 */
esp_err_t sync_time_with_ntp(const char* ssid, const char* password, 
                              const char* timezone, const char* ntp_server);

#endif // TIME_SYNC_H