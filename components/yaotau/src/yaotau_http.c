#include "yaotau_http.h"

#include <string.h>
#include <inttypes.h>  // Add this for PRIu32

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "cJSON.h"


#ifndef CONFIG_YAOTAU_SERVER_VERSION_URL
#pragma message("You need to define a CONFIG_YAOTAU_SERVER_VERSION_URL in the Kconfig hint: idf.py menuconfig")
#endif

static const char *TAG = "yaotau_http";

static char *response_buffer = NULL;
static int response_len = 0;
static int response_cap = 0;

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (!response_buffer) {
                response_cap = 1024;
                response_buffer = malloc(response_cap);
                if (!response_buffer) return ESP_FAIL;
                response_len = 0;
            }
            if (response_len + evt->data_len + 1 > response_cap) {
                response_cap = response_len + evt->data_len + 1;
                char *new_buf = realloc(response_buffer, response_cap);
                if (!new_buf) return ESP_FAIL;
                response_buffer = new_buf;
            }
            memcpy(response_buffer + response_len, evt->data, evt->data_len);
            response_len += evt->data_len;
            response_buffer[response_len] = '\0';
            break;
        case HTTP_EVENT_DISCONNECTED:
            // Optional: handle disconnect
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t yaotau_fetch_server_info(yaotau_server_info_t *out) {
    if (!out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));

    // Reset buffer
    if (response_buffer) {
        free(response_buffer);
        response_buffer = NULL;
        response_len = 0;
        response_cap = 0;
    }

    esp_http_client_config_t cfg = {
        .url = CONFIG_YAOTAU_SERVER_VERSION_URL,
        .timeout_ms = CONFIG_YAOTAU_HTTP_TIMEOUT_MS,
        .event_handler = http_event_handler,
    };

    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return ESP_FAIL;

    esp_err_t err = esp_http_client_perform(c);
    int status = esp_http_client_get_status_code(c);
    esp_http_client_cleanup(c);

    ESP_LOGI(TAG, "HTTP status=%d, response_len=%d", status, response_len);
    if (status != 200 || response_len <= 0) {
        if (response_buffer) free(response_buffer);
        response_buffer = NULL;
        response_len = 0;
        response_cap = 0;
        return ESP_ERR_INVALID_RESPONSE;
    }

    ESP_LOGI(TAG, "Fetched response: %.256s", response_buffer);

    cJSON *root = cJSON_Parse(response_buffer);
    free(response_buffer);
    response_buffer = NULL;
    response_len = 0;
    response_cap = 0;

    if (!root) {
        ESP_LOGW(TAG, "JSON parse failed");
        return ESP_ERR_INVALID_RESPONSE;
    }

    cJSON *ver = cJSON_GetObjectItemCaseSensitive(root, "version");
    cJSON *url = cJSON_GetObjectItemCaseSensitive(root, "image_url");
    if (!cJSON_IsString(ver) || !cJSON_IsString(url)) {
        cJSON_Delete(root);
        ESP_LOGW(TAG, "JSON missing expected keys or not strings");
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Handle empty version gracefully (log warning but proceed)
    if (strlen(ver->valuestring) == 0) {
        ESP_LOGW(TAG, "Server returned empty version string");
    }

    strncpy(out->version, ver->valuestring, sizeof(out->version) - 1);
    strncpy(out->image_url, url->valuestring, sizeof(out->image_url) - 1);
    cJSON_Delete(root);

    ESP_LOGI(TAG, "Server version='%s' image_url='%s'", out->version, out->image_url);
    return ESP_OK;
}

esp_err_t yaotau_http_ota_to_next_slot(const char *image_url) {
    if (!image_url) return ESP_ERR_INVALID_ARG;

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "No OTA partition available");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "OTA: will write to partition '%s'", update_partition->label);

    esp_http_client_config_t http_cfg = {
        .url = image_url,
        .timeout_ms = 30000,
        .keep_alive_enable = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    if (!client) return ESP_FAIL;

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return err;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0) {
        ESP_LOGE(TAG, "Failed to get content length");
        esp_http_client_cleanup(client);
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (content_length > update_partition->size) {
        ESP_LOGE(TAG, "Image too large: %d > %"PRIu32, content_length, update_partition->size);
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }

    esp_ota_handle_t ota_handle;
    err = esp_ota_begin(update_partition, content_length, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    ESP_LOGI(TAG, "Starting HTTP OTA from: %s", image_url);

    // Allocate buffer on heap to avoid stack overflow
    uint8_t *buf = malloc(4096);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate OTA buffer");
        esp_ota_abort(ota_handle);
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }

    int total_read = 0;
    while (total_read < content_length) {
        int len = esp_http_client_read(client, (char*)buf, 4096);
        if (len < 0) {
            ESP_LOGE(TAG, "HTTP read failed");
            free(buf);
            esp_ota_abort(ota_handle);
            esp_http_client_cleanup(client);
            return ESP_ERR_INVALID_RESPONSE;
        }
        if (len == 0) break;  // EOF

        err = esp_ota_write(ota_handle, buf, len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "OTA write failed: %s", esp_err_to_name(err));
            free(buf);
            esp_ota_abort(ota_handle);
            esp_http_client_cleanup(client);
            return err;
        }
        total_read += len;
    }

    free(buf);
    esp_http_client_cleanup(client);

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA end failed: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA set boot failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "HTTP OTA successful! Reboot to apply.");
    return ESP_OK;
}
