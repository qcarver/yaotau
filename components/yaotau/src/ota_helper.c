#include "ota_helper.h"

#include <inttypes.h>
#include <string.h>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "esp_app_format.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "yaotau_wifi.h"
#include "yaotau_http.h"
#include "yaotau_semver.h"

static const char *TAG = "yaotau";

static void log_banner(void) {
    const esp_app_desc_t *d = esp_app_get_description();
    char sha[65] = {0};
    for (int i = 0; i < 32; i++) {
        sprintf(&sha[i * 2], "%02x", d->app_elf_sha256[i]);
    }
    sha[64] = '\0';

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "App: %s", d->project_name);
    ESP_LOGI(TAG, "IDF App Ver: %s", d->version);
    ESP_LOGI(TAG, "YAOTA Ver:   %s", YAOTAU_BUILD_VERSION);
    ESP_LOGI(TAG, "SHA: %s", sha);
    ESP_LOGI(TAG, "========================================");
}

static void init_nvs_netif_eventloop(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS needs erase: %s", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(err);
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
}

static void log_partitions(const esp_partition_t *running, const esp_partition_t *next) {
    ESP_LOGI(TAG, "Running partition: label=%s addr=0x%"PRIx32" size=%"PRIu32,
             running ? running->label : "(null)",
             running ? running->address : 0,
             running ? running->size : 0);

    ESP_LOGI(TAG, "Next OTA slot:      label=%s addr=0x%"PRIx32" size=%"PRIu32,
             next ? next->label : "(null)",
             next ? next->address : 0,
             next ? next->size : 0);
}

esp_err_t ota_helper_init(void) {
#if !CONFIG_YAOTAU_ENABLE
    return ESP_OK;
#endif

    log_banner();

    // Non-fatal design: on failure, log and return so the main app can proceed.
    init_nvs_netif_eventloop();

    const esp_partition_t *running = esp_ota_get_running_partition();
    const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);

    if (!running || !next) {
        ESP_LOGW(TAG, "Could not determine OTA partitions (running=%p next=%p)", running, next);
        return ESP_OK;
    }
    log_partitions(running, next);

    if (yaotau_wifi_connect() != ESP_OK) {
        ESP_LOGW(TAG, "WiFi connect failed; skipping OTA check");
        return ESP_OK;
    }

    yaotau_server_info_t info = {0};
    esp_err_t err = yaotau_fetch_server_info(&info);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed fetching version.json: %s", esp_err_to_name(err));
        yaotau_wifi_disconnect();
        return ESP_OK;
    }

    // Compare YAOTAU_BUILD_VERSION (patched every build) to server.
    int cmp = yaotau_semver_compare(YAOTAU_BUILD_VERSION, info.version);
    ESP_LOGI(TAG, "Local ver=%s, Server ver=%s, cmp=%d", YAOTAU_BUILD_VERSION, info.version, cmp);

    if (cmp >= 0) {
        ESP_LOGI(TAG, "No update needed");
        yaotau_wifi_disconnect();
        return ESP_OK;
    }

    if ((uint32_t)CONFIG_YAOTAU_IMAGE_MAX_BYTES > 0 && (uint32_t)CONFIG_YAOTAU_IMAGE_MAX_BYTES > next->size) {
        ESP_LOGW(TAG, "Configured max image bytes (%u) exceeds OTA slot size (%"PRIu32")",
                 (unsigned)CONFIG_YAOTAU_IMAGE_MAX_BYTES, next->size);
        // Not fatal; we still try, but your server should respect the slot size.
    }

    ESP_LOGI(TAG, "Update available: downloading %s", info.image_url);

    err = yaotau_http_ota_to_next_slot(info.image_url);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "OTA failed: %s", esp_err_to_name(err));
        yaotau_wifi_disconnect();
        return ESP_OK;
    }

    ESP_LOGI(TAG, "OTA succeeded; rebooting into new image");
    yaotau_wifi_disconnect();
    esp_restart();
    return ESP_OK;
}
