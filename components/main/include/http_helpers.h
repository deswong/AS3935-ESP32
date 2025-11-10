#pragma once
#include <esp_err.h>

// forward-declare http request type to avoid including IDF http server
// header in every header file that just needs the prototype.
typedef struct httpd_req httpd_req_t;

// Small helper wrappers implemented in http_helpers.c
esp_err_t http_helpers_send_400(httpd_req_t *req);
esp_err_t http_helpers_send_500(httpd_req_t *req);
