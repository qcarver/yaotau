#include "yaotau_wifi.h"

#include <string.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "yaotau_wifi";

static EventGroupHandle_t s_evt;
static int s_retry;
static esp_netif_t *s_netif;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg; (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry < CONFIG_YAOTAU_WIFI_MAX_RETRY) {
            s_retry++;
            ESP_LOGW(TAG, "retry %d/%d", s_retry, CONFIG_YAOTAU_WIFI_MAX_RETRY);
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_evt, WIFI_FAIL_BIT);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_evt, WIFI_CONNECTED_BIT);
    }
}

esp_err_t yaotau_wifi_connect(void) {
    if (strlen(CONFIG_YAOTAU_WIFI_SSID) == 0) {
        ESP_LOGW(TAG, "No SSID configured");
        return ESP_ERR_INVALID_STATE;
    }

    s_evt = xEventGroupCreate();
    s_retry = 0;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    s_netif = esp_netif_create_default_wifi_sta();
    if (!s_netif) return ESP_FAIL;

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi = {0};
    strncpy((char *)wifi.sta.ssid, CONFIG_YAOTAU_WIFI_SSID, sizeof(wifi.sta.ssid));
    strncpy((char *)wifi.sta.password, CONFIG_YAOTAU_WIFI_PASSWORD, sizeof(wifi.sta.password));

    // Leave authmode flexible (if you need open networks, adjust this via Kconfig).
    wifi.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(
        s_evt, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(15000)
    );

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected");
        return ESP_OK;
    }

    ESP_LOGW(TAG, "WiFi not connected");
    return ESP_FAIL;
}

void yaotau_wifi_disconnect(void) {
    // Best-effort cleanup.
    esp_wifi_stop();
    esp_wifi_deinit();

    if (s_netif) {
        esp_netif_destroy(s_netif);
        s_netif = NULL;
    }
    if (s_evt) {
        vEventGroupDelete(s_evt);
        s_evt = NULL;
    }

    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler);
}
