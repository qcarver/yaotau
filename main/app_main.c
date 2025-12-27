#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ota_helper.h"
#include "yaota_version.h"   // generated each build in build/generated/

static const char *TAG = "app";

void app_main(void)
{
    ESP_LOGI(TAG, "yaota_demo starting; YAOTA_BUILD_VERSION=%s", YAOTA_BUILD_VERSION);

    // Non-fatal: returns regardless of outcome.
    ota_helper_init();

    // Your "real app" continues here.
    while (1) {
        ESP_LOGI(TAG, "main loop alive; version=%s", YAOTA_BUILD_VERSION);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
