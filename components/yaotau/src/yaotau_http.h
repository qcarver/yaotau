#pragma once

#include "esp_err.h"
#include "esp_partition.h"

typedef struct {
    char version[32];
    char image_url[256];
} yaotau_server_info_t;

esp_err_t yaotau_fetch_server_info(yaotau_server_info_t *out);
esp_err_t yaotau_http_ota_to_next_slot(const char *image_url);
