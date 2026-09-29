/* Wi-Fi Motion Detector (RSSI Tuning Mode)

   Monitors Wi-Fi signal strength (dBm) in real-time from Mobile Hotspot.
   Drives Buzzer (GPIO 18) when motion is detected.
*/

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

/* Hotspot & Hardware Pin Configuration */
#define DEFAULT_SSID          CONFIG_EXAMPLE_WIFI_SSID
#define DEFAULT_PWD           CONFIG_EXAMPLE_WIFI_PASSWORD
#define BUZZER_GPIO           CONFIG_MOTION_BUZZER_GPIO
#define MOTION_RSSI_THRESHOLD CONFIG_MOTION_RSSI_THRESHOLD
#define POLL_INTERVAL_MS      CONFIG_MOTION_POLL_INTERVAL_MS

#if CONFIG_EXAMPLE_WIFI_ALL_CHANNEL_SCAN
#define DEFAULT_SCAN_METHOD WIFI_ALL_CHANNEL_SCAN
#elif CONFIG_EXAMPLE_WIFI_FAST_SCAN
#define DEFAULT_SCAN_METHOD WIFI_FAST_SCAN
#else
#define DEFAULT_SCAN_METHOD WIFI_FAST_SCAN
#endif

#if CONFIG_EXAMPLE_WIFI_CONNECT_AP_BY_SIGNAL
#define DEFAULT_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#elif CONFIG_EXAMPLE_WIFI_CONNECT_AP_BY_SECURITY
#define DEFAULT_SORT_METHOD WIFI_CONNECT_AP_BY_SECURITY
#else
#define DEFAULT_SORT_METHOD WIFI_CONNECT_AP_BY_SIGNAL
#endif

#if CONFIG_EXAMPLE_FAST_SCAN_THRESHOLD
#define DEFAULT_RSSI CONFIG_EXAMPLE_FAST_SCAN_MINIMUM_SIGNAL
#if CONFIG_EXAMPLE_FAST_SCAN_WEAKEST_AUTHMODE_OPEN
#define DEFAULT_AUTHMODE WIFI_AUTH_OPEN
#elif CONFIG_EXAMPLE_FAST_SCAN_WEAKEST_AUTHMODE_WEP
#define DEFAULT_AUTHMODE WIFI_AUTH_WEP
#elif CONFIG_EXAMPLE_FAST_SCAN_WEAKEST_AUTHMODE_WPA
#define DEFAULT_AUTHMODE WIFI_AUTH_WPA_PSK
#elif CONFIG_EXAMPLE_FAST_SCAN_WEAKEST_AUTHMODE_WPA2
#define DEFAULT_AUTHMODE WIFI_AUTH_WPA2_PSK
#else
#define DEFAULT_AUTHMODE WIFI_AUTH_OPEN
#endif
#else
#define DEFAULT_RSSI -127
#define DEFAULT_AUTHMODE WIFI_AUTH_OPEN
#endif

static const char *TAG = "motion_detector";
static TaskHandle_t s_motion_detector_task = NULL;

static void motion_detector_task(void *param)
{
    ESP_LOGI(TAG, "=======================================================");
    ESP_LOGI(TAG, "  RSSI Motion Detector Active! Threshold = %d dBm  ", MOTION_RSSI_THRESHOLD);
    ESP_LOGI(TAG, "  Buzzer: GPIO %d                    ", BUZZER_GPIO);
    ESP_LOGI(TAG, "=======================================================");

    while (1) {
        wifi_ap_record_t ap;
        esp_err_t err = esp_wifi_sta_get_ap_info(&ap);
        if (err == ESP_OK) {
            int rssi = ap.rssi;
            bool motion_detected = rssi < MOTION_RSSI_THRESHOLD;

            gpio_set_level(BUZZER_GPIO, motion_detected);

            if (motion_detected) {
                ESP_LOGW(TAG, "🚨 [MOTION DETECTED] RSSI: %d dBm  <  Threshold (%d dBm) -> BUZZER: ON",
                         rssi, MOTION_RSSI_THRESHOLD);
            } else {
                ESP_LOGI(TAG, "🟢 [NORMAL SIGNAL]   RSSI: %d dBm  >= Threshold (%d dBm) -> BUZZER: OFF",
                         rssi, MOTION_RSSI_THRESHOLD);
            }
        } else {
            ESP_LOGE(TAG, "Failed to read Wi-Fi signal info (Error: %d)", err);
            gpio_set_level(BUZZER_GPIO, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        gpio_set_level(BUZZER_GPIO, 0);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connected to Hotspot! Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        if (s_motion_detector_task == NULL) {
            xTaskCreate(motion_detector_task, "motion_detector", 4096, NULL, 5,
                        &s_motion_detector_task);
        }
    }
}

static void init_wifi_scan(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, NULL));

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = DEFAULT_SSID,
            .password = DEFAULT_PWD,
            .scan_method = DEFAULT_SCAN_METHOD,
            .sort_method = DEFAULT_SORT_METHOD,
            .threshold.rssi = DEFAULT_RSSI,
            .threshold.authmode = DEFAULT_AUTHMODE,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main(void)
{
    const gpio_config_t gpio_cfg = {
        .pin_bit_mask = (1ULL << BUZZER_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&gpio_cfg));
    ESP_ERROR_CHECK(gpio_set_level(BUZZER_GPIO, 0));

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    init_wifi_scan();
}
