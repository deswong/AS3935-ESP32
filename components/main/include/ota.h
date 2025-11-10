// Simple OTA helper API
#pragma once
#include <esp_err.h>
#include "esp_http_server.h"

// HTTP handler to trigger OTA: expects POST JSON { "url": "https://..." }
esp_err_t ota_start_handler(httpd_req_t *req);
