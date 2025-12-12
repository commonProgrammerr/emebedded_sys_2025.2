#ifndef ALERTS_H
#define ALERTS_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>

typedef enum {
    ALERT_NONE = 0,
    ALERT_WARNING,
    ALERT_CRITICAL
} alert_t;

extern alert_t alert_status;

esp_err_t alerts_init();
esp_err_t alerts_send_alert(alert_t type, const char* message);
esp_err_t alerts_clear_alert();
esp_err_t alerts_snooze(uint32_t duration_ms);

#endif