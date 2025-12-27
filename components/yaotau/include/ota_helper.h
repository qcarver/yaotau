#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Checks server for a newer version and, if found, downloads to the next OTA slot and reboots.
 *
 * Call once near the top of app_main(), after logging is initialized.
 * Non-fatal behavior: logs failures and returns so your normal app flow continues.
 */
esp_err_t ota_helper_init(void);

#ifdef __cplusplus
}
#endif
