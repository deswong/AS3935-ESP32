#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#include "esp_system.h"
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include "esp_netif.h"

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/apps/sntp.h"

#include "settings.h"
#include "wifi_prov.h"
#include "cJSON.h"
#include "http_helpers.h"

static const char *TAG = "wifi_prov";
static TaskHandle_t captive_dns_task = NULL;
static TaskHandle_t wifi_reconnect_task_handle = NULL;
static TaskHandle_t sntp_task_handle = NULL;
static esp_netif_t *s_sta_netif = NULL;
static int s_retry_num = 0;
static const int MAX_RETRY = 5;
static bool s_connected = false;
static bool s_ap_active = false;
static bool s_fallback_to_ap_triggered = false;  // Track if we already started AP fallback

// Register/set the STA netif that was created elsewhere (e.g., in app_main)
void wifi_prov_register_sta_netif(esp_netif_t *sta_netif)
{
    s_sta_netif = sta_netif;
    ESP_LOGI(TAG, "Registered STA netif for APSTA mode support");
}

// Forward declaration of connect task
static void wifi_connect_task(void *arg);
static void wifi_reconnect_task(void *arg);
static void initialize_sntp(void);
static void sntp_resync_task(void *arg);

// DNS responder: craft a simple DNS A response mapping any query to 192.168.4.1
static void captive_dns_task_fn(void *arg)
{
    (void)arg;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        vTaskDelete(NULL);
        return;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(53);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind(sock, (struct sockaddr*)&addr, sizeof(addr));

    while (s_ap_active) {
        uint8_t buf[512];
        struct sockaddr_in client;
        socklen_t len = sizeof(client);
        int r = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&client, &len);
        if (r > 12) {
            // Build response in-place (simple A record pointing to 192.168.4.1)
            buf[2] |= 0x80; // QR
            buf[3] |= 0x80; // RA
            buf[6] = 0x00; buf[7] = 0x01; // ANCOUNT = 1
            int resp_len = r;
            buf[resp_len++] = 0xC0; buf[resp_len++] = 0x0C; // name pointer
            buf[resp_len++] = 0x00; buf[resp_len++] = 0x01; // type A
            buf[resp_len++] = 0x00; buf[resp_len++] = 0x01; // class IN
            buf[resp_len++] = 0x00; buf[resp_len++] = 0x00; buf[resp_len++] = 0x00; buf[resp_len++] = 0x3C; // TTL
            buf[resp_len++] = 0x00; buf[resp_len++] = 0x04; // RDLENGTH
            buf[resp_len++] = 192; buf[resp_len++] = 168; buf[resp_len++] = 4; buf[resp_len++] = 1; // 192.168.4.1
            sendto(sock, buf, resp_len, 0, (struct sockaddr*)&client, len);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    close(sock);
    vTaskDelete(NULL);
}

static void start_captive_dns(void)
{
    if (captive_dns_task == NULL) {
        xTaskCreate(captive_dns_task_fn, "captive_dns", 4096, NULL, 5, &captive_dns_task);
        s_ap_active = true;
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retrying to connect (%d/%d)", s_retry_num, MAX_RETRY);
        } else if (!s_fallback_to_ap_triggered) {
            s_fallback_to_ap_triggered = true;
            ESP_LOGW(TAG, "Max retries reached, falling back to AP");
            // Start AP without stopping STA so we can continue reconnect attempts in background
            wifi_prov_start_ap("AS3935-Setup");
            start_captive_dns();
            // start a background reconnect task if not already running
            if (wifi_reconnect_task_handle == NULL) {
                xTaskCreate(wifi_reconnect_task, "wifi_reconnect", 4096, NULL, 5, &wifi_reconnect_task_handle);
            }
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* ev = (ip_event_got_ip_t*) event_data;
        s_retry_num = 0;
        s_connected = true;
        ESP_LOGI(TAG, "Got IP: %s", ip4addr_ntoa((const ip4_addr_t*)&ev->ip_info.ip));
        /* Initialize SNTP when we have a network connection and an IP address. */
        initialize_sntp();
        // If AP fallback was active, disable AP to return to STA-only operation
        if (s_ap_active) {
            ESP_LOGI(TAG, "Stopping fallback AP, returning to STA-only mode");
            s_ap_active = false;
            // switch to STA only
            esp_wifi_set_mode(WIFI_MODE_STA);
            // stop captive DNS task if running
            if (captive_dns_task) {
                // let captive_dns_task see s_ap_active==false and exit
                // wait briefly for it to terminate
                int wait = 0;
                while (captive_dns_task && wait++ < 50) vTaskDelay(pdMS_TO_TICKS(20));
                captive_dns_task = NULL;
            }
            // if a reconnect task is running, it will exit when s_connected==true
        }
    }
}

// background reconnect task - keeps calling esp_wifi_connect until s_connected is true
static void wifi_reconnect_task(void *arg)
{
    (void)arg;
    while (!s_connected) {
        ESP_LOGI(TAG, "Background reconnect attempt...");
        esp_wifi_connect();
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
    // clear handle before exiting
    wifi_reconnect_task_handle = NULL;
    vTaskDelete(NULL);
}

// Watchdog task to detect if WiFi is stuck in state machine loop without triggering disconnect events
static TaskHandle_t wifi_timeout_task_handle = NULL;
static void wifi_connect_timeout_task(void *arg)
{
    (void)arg;
    // Wait up to 15 seconds for connection or disconnect event
    int timeout_ms = 15000;
    int check_interval_ms = 1000;
    int elapsed = 0;
    
    while (elapsed < timeout_ms && !s_connected && !s_fallback_to_ap_triggered) {
        vTaskDelay(pdMS_TO_TICKS(check_interval_ms));
        elapsed += check_interval_ms;
    }
    
    // If we timed out (didn't connect and didn't get disconnect event), force fallback
    if (!s_connected && !s_fallback_to_ap_triggered) {
        ESP_LOGW(TAG, "WiFi connection timeout (%d ms) - forcing fallback to AP", timeout_ms);
        s_fallback_to_ap_triggered = true;
        wifi_prov_start_ap("AS3935-Setup");
        start_captive_dns();
        if (wifi_reconnect_task_handle == NULL) {
            xTaskCreate(wifi_reconnect_task, "wifi_reconnect", 4096, NULL, 5, &wifi_reconnect_task_handle);
        }
    }
    
    // clear handle and exit
    wifi_timeout_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t wifi_prov_init(void)
{
    // NOTE: esp_netif_init() and esp_event_loop_create_default() are called from app_main
    // DO NOT reinitialize them here - it causes undefined behavior in ESP-IDF v6
    // Only register event handlers here
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));
    return ESP_OK;
}

esp_err_t wifi_prov_start_ap(const char *ap_ssid)
{
    esp_netif_create_default_wifi_ap();
    wifi_config_t wifi_ap_config = {0};
    strncpy((char*)wifi_ap_config.ap.ssid, ap_ssid, sizeof(wifi_ap_config.ap.ssid)-1);
    wifi_ap_config.ap.ssid_len = strlen(ap_ssid);
    wifi_ap_config.ap.max_connection = 4;
    wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    // If STA is already initialized, use APSTA mode so STA remains active
    wifi_mode_t mode = WIFI_MODE_AP;
    if (s_sta_netif) mode = WIFI_MODE_APSTA;
    esp_wifi_set_mode(mode);
    esp_err_t err = esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err == ESP_OK) s_ap_active = true;
    ESP_LOGI(TAG, "Started AP '%s' (err=%d)", ap_ssid, err);
    // Start captive DNS so clients get the captive-portal UX immediately
    if (err == ESP_OK) {
        start_captive_dns();
    }
    return err;
}

// Initialize SNTP (NTP) client, point at pool.ntp.org by default.
static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_stop();
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
#if defined(CONFIG_LWIP_SNTP_UPDATE_DELAY)
    /* If LWIP SNTP sync interval is configurable, leave it; otherwise we
       schedule an explicit resync task below. */
#endif
    sntp_init();

    /* Start a background task to restart SNTP every 12 hours to limit drift. */
    if (sntp_task_handle == NULL) {
        xTaskCreate(sntp_resync_task, "sntp_resync", 4096, NULL, 5, &sntp_task_handle);
    }
}

static void sntp_resync_task(void *arg)
{
    (void)arg;
    const TickType_t delay = pdMS_TO_TICKS(12 * 60 * 60 * 1000UL); // 12 hours
    while (1) {
        vTaskDelay(delay);
        ESP_LOGI(TAG, "SNTP resync: restarting SNTP to reduce clock drift");
        sntp_stop();
        /* reconfigure server in case configuration was changed elsewhere */
        sntp_setservername(0, "pool.ntp.org");
        sntp_init();
    }
    vTaskDelete(NULL);
}

void wifi_prov_start_connect_with_fallback(void)
{
    char ssid[64] = {0};
    char password[64] = {0};
    if (settings_load_str("wifi", "ssid", ssid, sizeof(ssid)) != ESP_OK) {
        ESP_LOGI(TAG, "No saved SSID to connect");
        return;
    }
    settings_load_str("wifi", "password", password, sizeof(password));

    // STA netif must already be created by app_main and registered via wifi_prov_register_sta_netif
    if (!s_sta_netif) {
        ESP_LOGW(TAG, "STA netif not registered! Did app_main call wifi_prov_register_sta_netif?");
        return;
    }
    
    // Set STA mode (don't call esp_wifi_init again - it's already called from app_main)
    esp_wifi_set_mode(WIFI_MODE_STA);

    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid)-1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password)-1);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    s_retry_num = 0;
    
    // Start timeout task to detect if connection gets stuck in state machine
    if (wifi_timeout_task_handle == NULL) {
        xTaskCreate(wifi_connect_timeout_task, "wifi_timeout", 2048, NULL, 5, &wifi_timeout_task_handle);
        ESP_LOGI(TAG, "Started WiFi connection timeout watchdog (15s)");
    }
}

esp_err_t wifi_status_handler(httpd_req_t *req)
{
    char ssid[64] = {0};
    settings_load_str("wifi", "ssid", ssid, sizeof(ssid));
    char pwd[64] = {0};
    settings_load_str("wifi", "password", pwd, sizeof(pwd));
    char buf[256];
    int pwd_set = pwd[0] ? 1 : 0;
    if (s_connected && s_sta_netif) {
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(s_sta_netif, &ip_info);
        snprintf(buf, sizeof(buf), "{\"connected\":true, \"ssid\":\"%s\", \"ip\":\"%s\", \"password_set\":%s}", ssid, ip4addr_ntoa((const ip4_addr_t*)&ip_info.ip), pwd_set?"true":"false");
    } else {
        snprintf(buf, sizeof(buf), "{\"connected\":false, \"ssid\":\"%s\", \"password_set\":%s}", ssid, pwd_set?"true":"false");
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    // Start WiFi scan
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    
    esp_wifi_scan_start(&scan_config, true);  // blocking scan
    
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    if (ap_count == 0) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }
    
    wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (!ap_list) {
        http_helpers_send_500(req);
        return ESP_ERR_NO_MEM;
    }
    
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_list));
    
    // Build JSON manually to avoid cJSON compatibility issues
    char *buffer = malloc(8192);
    if (!buffer) {
        free(ap_list);
        http_helpers_send_500(req);
        return ESP_ERR_NO_MEM;
    }
    
    int offset = 0;
    int n = snprintf(buffer + offset, 8192 - offset, "[");
    if (n < 0 || n >= 8192 - offset) {
        // Truncated or error
        free(ap_list);
        http_helpers_send_500(req);
        free(buffer);
        return ESP_FAIL;
    }
    offset += n;
    
    for (int i = 0; i < ap_count && offset < 8000; i++) {
        // Escape quotes in SSID
        char ssid_escaped[65] = {0};
        int j = 0;
        for (int k = 0; k < 32 && ap_list[i].ssid[k] && j < 63; k++) {
            if (ap_list[i].ssid[k] == '"' || ap_list[i].ssid[k] == '\\') {
                ssid_escaped[j++] = '\\';
            }
            ssid_escaped[j++] = ap_list[i].ssid[k];
        }
        
        if (i > 0) {
            n = snprintf(buffer + offset, 8192 - offset, ",");
            if (n < 0 || n >= 8192 - offset) {
                free(ap_list);
                http_helpers_send_500(req);
                free(buffer);
                return ESP_FAIL;
            }
            offset += n;
        }
        n = snprintf(buffer + offset, 8192 - offset, 
                "{\"ssid\":\"%s\",\"rssi\":%d,\"channel\":%d}",
                ssid_escaped, ap_list[i].rssi, ap_list[i].primary);
        if (n < 0 || n >= 8192 - offset) {
            free(ap_list);
            http_helpers_send_500(req);
            free(buffer);
            return ESP_FAIL;
        }
        offset += n;
    }
    
    n = snprintf(buffer + offset, 8192 - offset, "]");
    if (n < 0 || n >= 8192 - offset) {
        free(ap_list);
        http_helpers_send_500(req);
        free(buffer);
        return ESP_FAIL;
    }
    offset += n;
    
    free(ap_list);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buffer);
    free(buffer);
    
    return ESP_OK;
}

esp_err_t wifi_save_handler(httpd_req_t *req)
{
    int content_len = req->content_len;
    if (content_len <= 0 || content_len > 1024) {
        http_helpers_send_400(req);
        return ESP_FAIL;
    }
    char *buf = malloc(content_len + 1);
    if (!buf) { http_helpers_send_500(req); return ESP_ERR_NO_MEM; }
    int ret = httpd_req_recv(req, buf, content_len);
    if (ret <= 0) { free(buf); http_helpers_send_500(req); return ESP_FAIL; }
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) { http_helpers_send_400(req); return ESP_FAIL; }
    const cJSON *ssid = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    const cJSON *password = cJSON_GetObjectItemCaseSensitive(root, "password");
    if (!cJSON_IsString(ssid) || (ssid->valuestring == NULL)) {
        cJSON_Delete(root);
        http_helpers_send_400(req);
        return ESP_FAIL;
    }
    const char *pwd = NULL;
    if (cJSON_IsString(password) && password->valuestring) pwd = password->valuestring;

    settings_save_str("wifi", "ssid", ssid->valuestring);
    if (pwd) settings_save_str("wifi", "password", pwd);
    cJSON_Delete(root);

    // Start connect flow with event-driven retries and fallback
    xTaskCreate(&wifi_connect_task, "wifi_connect", 4096, NULL, 5, NULL);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}\n");
    return ESP_OK;
}

// connect task implementation
static void wifi_connect_task(void *arg)
{
    (void)arg;
    char ssid[64] = {0};
    char password[64] = {0};
    if (settings_load_str("wifi", "ssid", ssid, sizeof(ssid)) != ESP_OK) {
        ESP_LOGI(TAG, "No saved SSID");
        vTaskDelete(NULL);
        return;
    }
    settings_load_str("wifi", "password", password, sizeof(password));

    // Create STA netif only if not already created
    if (!s_sta_netif) {
        s_sta_netif = esp_netif_create_default_wifi_sta();
    }
    esp_netif_t *netif = s_sta_netif;

    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) { ESP_LOGE(TAG, "esp_wifi_set_mode failed: %d", err); vTaskDelete(NULL); return; }

    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid)-1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password)-1);
    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) { ESP_LOGE(TAG, "esp_wifi_set_config failed: %d", err); vTaskDelete(NULL); return; }
    err = esp_wifi_start();
    if (err != ESP_OK) { ESP_LOGE(TAG, "esp_wifi_start failed: %d", err); vTaskDelete(NULL); return; }

    const int max_retries = 5;
    int attempt = 0;
    while (attempt++ < max_retries) {
        ESP_LOGI(TAG, "Attempt %d to connect to '%s'", attempt, ssid);
        esp_err_t cerr = esp_wifi_connect();
        if (cerr != ESP_OK) {
            ESP_LOGW(TAG, "esp_wifi_connect returned %d", cerr);
        }
        vTaskDelay(pdMS_TO_TICKS(8000));
        // check IP assigned via esp_netif
        if (netif) {
            esp_netif_ip_info_t ip_info;
            if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
                if (ip_info.ip.addr != 0) {
                    ESP_LOGI(TAG, "Connected, IP: %s", ip4addr_ntoa((const ip4_addr_t*)&ip_info.ip));
                    vTaskDelete(NULL);
                    return;
                }
            }
        }
    }

    ESP_LOGW(TAG, "Failed to connect after %d attempts, starting AP for reconfiguration", max_retries);
    wifi_prov_start_ap("AS3935-Setup");
    start_captive_dns();
    vTaskDelete(NULL);
}


// Clean single implementation retained above. The duplicated code blocks and lambda were removed.
