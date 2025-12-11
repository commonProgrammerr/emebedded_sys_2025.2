#include "time_sync.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_netif.h"

time_t now;
char strftime_buf[64];
struct tm timeinfo;

static const char* TAG = "time_sync";
static EventGroupHandle_t wifi_event_group;
static const int WIFI_CONNECTED_BIT = BIT0;
static const int WIFI_FAIL_BIT = BIT1;
static int s_retry_num = 0;
#define MAX_RETRY_ATTEMPTS 5

static wifi_config_t wifi_config = {
        .sta = {
            .ssid = {0},
            .password = {0},
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_RETRY_ATTEMPTS) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Tentando reconectar ao WiFi (%d/%d)...", s_retry_num, MAX_RETRY_ATTEMPTS);
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGE(TAG, "Falha ao conectar ao WiFi após %d tentativas", MAX_RETRY_ATTEMPTS);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Endereço IP obtido: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t connect_wifi(const char* ssid, const char* password) {
    // Initialize network interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    
    // Create event group
    wifi_event_group = xEventGroupCreate();
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    
    // Setup WiFi as Station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    // Configure WiFi connection settings
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    
    // Start WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait for connection
    ESP_LOGI(TAG, "Conectando ao WiFi SSID: %s...", ssid);
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Conectado ao WiFi SSID: %s", ssid);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Falha ao conectar ao WiFi SSID: %s", ssid);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "Evento inesperado ao conectar WiFi");
        return ESP_ERR_TIMEOUT;
    }
}

void shutdown_wifi() {
    if (wifi_event_group) {
        vEventGroupDelete(wifi_event_group);
        wifi_event_group = NULL;
    }
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, NULL));
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(esp_event_loop_delete_default());
    esp_netif_deinit();
    ESP_LOGI(TAG, "WiFi desligado");
}

esp_err_t init_sntp_sync(const char* timezone, const char* ntp_server)
{
    // Set timezone
    setenv("TZ", timezone ? timezone : "BRT3", 1);
    tzset();
    
    // Initialize SNTP
    ESP_LOGI(TAG, "Inicializando sincronização SNTP...");
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(ntp_server ? ntp_server : "a.st1.ntp.br");
    esp_err_t ret = esp_netif_sntp_init(&config);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar SNTP: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Wait for time to be synchronized (with timeout)
    int retry = 0;
    const int retry_count = 15;
    while (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(2000)) == ESP_ERR_TIMEOUT && ++retry < retry_count) {
        ESP_LOGI(TAG, "Aguardando sincronização SNTP... (%d/%d)", retry, retry_count);
    }
    
    if (retry >= retry_count) {
        ESP_LOGW(TAG, "Timeout na sincronização SNTP");
        return ESP_ERR_TIMEOUT;
    }
    
    // Get and display current time
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "Hora sincronizada: %s", strftime_buf);
    
    return ESP_OK;
}

esp_err_t sync_time_with_ntp(const char* ssid, const char* password, 
                              const char* timezone, const char* ntp_server)
{
    esp_err_t ret;
    
    // Connect to WiFi
    ret = connect_wifi(ssid, password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao conectar WiFi, abortando sincronização SNTP");
        return ret;
    }
    
    // Initialize and sync SNTP
    ret = init_sntp_sync(timezone, ntp_server);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha na sincronização SNTP");
        shutdown_wifi();
        return ret;
    }
    
    ESP_LOGI(TAG, "Sincronização de tempo concluída com sucesso");
    return ESP_OK;
}