#pragma once
#include <esp_err.h>
#include "esp_http_server.h"
#include "esp_netif.h"

esp_err_t wifi_prov_init(void);
esp_err_t wifi_prov_start_ap(const char *ap_ssid);
// starts a background connect attempt with retries; falls back to AP on failure
void wifi_prov_start_connect_with_fallback(void);
// Register STA netif (must be called before start_ap for APSTA mode to work)
void wifi_prov_register_sta_netif(esp_netif_t *sta_netif);

// HTTP handlers
esp_err_t wifi_status_handler(httpd_req_t *req);
esp_err_t wifi_save_handler(httpd_req_t *req);
esp_err_t wifi_scan_handler(httpd_req_t *req);

