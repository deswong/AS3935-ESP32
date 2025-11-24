#pragma once

// Use the ESP-IDF-provided http server types to avoid mismatches / duplicate typedefs
#include <esp_http_server.h>
#include "esp_err.h"

// Small helper wrappers implemented in http_helpers.c
esp_err_t http_helpers_send_400(httpd_req_t *req);
esp_err_t http_helpers_send_500(httpd_req_t *req);
esp_err_t http_reply_json(httpd_req_t *req, const char *json);
