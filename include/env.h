#ifndef PROJECT_ENV_H
#define PROJECT_ENV_H
#include "sdkconfig.h"


/* System Pinout */

// Noise Sensor (KY-037)
#define MIC_ADC_CHANEL ADC_CHANNEL_5

// Temperature and Humidity Sensor (DHT11)
#define DHT11_PIN 23

// Light Sensor (BH1750FVI) - I2C Pins
#if defined(CONFIG_IDF_TARGET_ESP32S2)
#define SCL_IO GPIO_NUM_20 
#define SDA_IO GPIO_NUM_21
#elif defined(CONFIG_IDF_TARGET_ESP32)
#define SDA_IO GPIO_NUM_18
#define SCL_IO GPIO_NUM_19
#endif

// User Button
#define BUTTON_PIN 22

// Alert System
#define BUZZER_GPIO GPIO_NUM_15         // Buzzer Pin
#define BUZZER_TIMER LEDC_TIMER_0       // PWM Timer for Buzzer
#define BUZZER_CHANNEL LEDC_CHANNEL_0   // Buzzer
#define CRITICAL_STATE_GPIO GPIO_NUM_26 // Red LED
#define WARNING_STATE_GPIO GPIO_NUM_32  // Yellow LED


/* --- System Intervals --- */

// Polling Intervals 
#define DHT11_READ_INTERVAL_MS 2000
#define BH1750_READ_INTERVAL_MS 10000
#define NOISE_READ_INTERVAL_MS 1000

// Button timers
#define DEBOUNCE_TIME_MS 50
#define LONG_PRESS_MS 2000
#define NORMAL_PRESS_MS 500

// Default Snooze Duration
#define SNOOZE_DURATION_MS 300000 // 5 minutes

#define NIGHT_START_HOUR 18 // 6 PM
#define NIGHT_END_HOUR 6    // 6 AM
/* --- Vivarium Limits (MVP Spec) --- */

// Temperature: 22-26°C
#define TEMP_MIN 22.0
#define TEMP_MAX 26.0

// Humidity: 40-60%
#define HUM_MIN  40.0
#define HUM_MAX  60.0

// Light: Day 150-300 lux, Night ~0
#define LUX_DAY_MIN 150
#define LUX_DAY_MAX 300
#define LUX_NIGHT_MIN 0
#define LUX_NIGHT_MAX 5 // Tolerance for light leakage

// Noise (RMS value, range 0-100%)
#define NOISE_PEAK_LIMIT 40
#define NOISE_AVG_LIMIT  21

/* --- Alert System Parameters --- */

#define DHT_WINDOW_WARNING_TOLERANCE 30 // 5 min (30 * 10s) 
#define DHT_WINDOW_CRITICAL_TOLERANCE 60 // 10 min (60 * 10s) 
#define LIGHT_WINDOW_WARNING_TOLERANCE 12 // 2 min (12 * 10s) 
#define LIGHT_WINDOW_CRITICAL_TOLERANCE 30 // 5 min (30 * 10s)
#define NOISE_PEAK_MAX_DURATION 3 // 3 seconds
#define NOISE_WINDOW_WARNING_TOLERANCE 40 // 40 seconds 



#endif // PROJECT_ENV_H