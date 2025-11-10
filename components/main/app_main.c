#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "as3935.h"
#include "settings.h"
#include "app_mqtt.h"
#include "events.h"
#include "wifi_prov.h"

static const char *TAG = "as3935_app";

// Simple handler to serve embedded index page
esp_err_t index_get_handler(httpd_req_t *req)
{
    extern const char index_html[];
    extern const size_t index_html_len;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html, index_html_len);
    return ESP_OK;
}

static httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_get_handler,
    .user_ctx = NULL
};

static httpd_uri_t wifi_status_uri = {
    .uri = "/api/wifi/status",
    .method = HTTP_GET,
    .handler = wifi_status_handler,
    .user_ctx = NULL
};

static httpd_uri_t wifi_save_uri = {
    .uri = "/api/wifi/save",
    .method = HTTP_POST,
    .handler = wifi_save_handler,
    .user_ctx = NULL
};

static httpd_uri_t mqtt_save_uri = {
    .uri = "/api/mqtt/save",
    .method = HTTP_POST,
    .handler = mqtt_save_handler,
    .user_ctx = NULL
};

static httpd_uri_t mqtt_status_uri = {
    .uri = "/api/mqtt/status",
    .method = HTTP_GET,
    .handler = mqtt_status_handler,
    .user_ctx = NULL
};

static httpd_uri_t mqtt_test_uri = {
    .uri = "/api/mqtt/test",
    .method = HTTP_POST,
    .handler = mqtt_test_publish_handler,
    .user_ctx = NULL
};

static httpd_uri_t mqtt_clear_uri = {
    .uri = "/api/mqtt/clear_credentials",
    .method = HTTP_POST,
    .handler = mqtt_clear_credentials_handler,
    .user_ctx = NULL
};


static httpd_uri_t as3935_save_uri = {
    .uri = "/api/as3935/save",
    .method = HTTP_POST,
    .handler = as3935_save_handler,
    .user_ctx = NULL
};

static httpd_uri_t as3935_status_uri = {
    .uri = "/api/as3935/status",
    .method = HTTP_GET,
    .handler = as3935_status_handler,
    .user_ctx = NULL
};

static httpd_uri_t as3935_pins_save_uri = {
    .uri = "/api/as3935/pins/save",
    .method = HTTP_POST,
    .handler = as3935_pins_save_handler,
    .user_ctx = NULL
};

static httpd_uri_t as3935_pins_status_uri = {
    .uri = "/api/as3935/pins/status",
    .method = HTTP_GET,
    .handler = as3935_pins_status_handler,
    .user_ctx = NULL
};


static httpd_uri_t as3935_params_uri = {
    .uri = "/api/as3935/params",
    .method = HTTP_POST,
    .handler = as3935_params_handler,
    .user_ctx = NULL
};

static httpd_uri_t as3935_calibrate_start_uri = {
    .uri = "/api/as3935/calibrate",
    .method = HTTP_POST,
    .handler = as3935_calibrate_start_handler,
    .user_ctx = NULL
};

static httpd_uri_t as3935_calibrate_status_uri = {
    .uri = "/api/as3935/calibrate/status",
    .method = HTTP_GET,
    .handler = as3935_calibrate_status_handler,
    .user_ctx = NULL
};

static httpd_uri_t as3935_calibrate_cancel_uri = {
    .uri = "/api/as3935/calibrate/cancel",
    .method = HTTP_POST,
    .handler = as3935_calibrate_cancel_handler,
    .user_ctx = NULL
};

static httpd_uri_t as3935_calibrate_apply_uri = {
    .uri = "/api/as3935/calibrate/apply",
    .method = HTTP_POST,
    .handler = as3935_calibrate_apply_handler,
    .user_ctx = NULL
};

static httpd_uri_t as3935_reg_uri = {
    .uri = "/api/as3935/reg",
    .method = HTTP_GET,
    .handler = as3935_reg_read_handler,
    .user_ctx = NULL
};

static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &index_uri);
    }
    return server;
}

// Captive portal redirect handler: redirect all unknown paths to /
static esp_err_t captive_redirect_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

static httpd_uri_t captive_redirect_uri = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = captive_redirect_handler,
    .user_ctx = NULL
};

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize wifi provisioning (event handlers)
    ESP_ERROR_CHECK(wifi_prov_init());

    // TODO: implement provisioning: start AP/captive portal on first boot and store wifi creds


    // Initialize settings (NVS)
    ESP_ERROR_CHECK(settings_init());

    // Load MQTT settings from NVS (including credentials and CA)
    char mqtt_uri[256] = {0};
    if (settings_load_str("mqtt", "uri", mqtt_uri, sizeof(mqtt_uri)) == ESP_OK) {
        char tls_str[8] = {0};
        settings_load_str("mqtt", "tls", tls_str, sizeof(tls_str));
        char username_saved[128] = {0};
        settings_load_str("mqtt", "username", username_saved, sizeof(username_saved));
        char password_saved[128] = {0};
        settings_load_str("mqtt", "password", password_saved, sizeof(password_saved));
        char ca_saved[2048] = {0};
        settings_load_str("mqtt", "ca_cert", ca_saved, sizeof(ca_saved));
        mqtt_config_t mcfg = { .uri = mqtt_uri, .use_tls = (tls_str[0]=='1'), .client_id = "as3935_esp32", .username = username_saved[0]?username_saved:NULL, .password = password_saved[0]?password_saved:NULL, .ca_cert = ca_saved[0]?ca_saved:NULL };
        mqtt_init(&mcfg);
    } else {
        ESP_LOGI(TAG, "No MQTT URI configured in NVS");
    }

    // Decide whether to start AP provisioning or attempt STA connect using new event-driven flow
    char ssid[64] = {0};
    httpd_handle_t server = start_webserver();
    if (settings_load_str("wifi", "ssid", ssid, sizeof(ssid)) == ESP_OK) {
        ESP_LOGI(TAG, "Found saved SSID '%s' - attempting STA connect", ssid);
        // start connect with retries and fallback to AP on failure
        wifi_prov_start_connect_with_fallback();
    } else {
        ESP_LOGI(TAG, "No saved wifi - starting provisioning AP");
        // Start AP mode for provisioning
        wifi_prov_start_ap("AS3935-Setup");
        // start captive DNS is done by wifi_prov on fallback if needed
    }

    // Initialize AS3935 with default pins (will be configured via UI later)
    as3935_config_t cfg = {0};
    as3935_init(&cfg);
    as3935_configure_default();

    // register endpoints once
    if (server) {
        httpd_register_uri_handler(server, &wifi_status_uri);
        httpd_register_uri_handler(server, &wifi_save_uri);
        httpd_register_uri_handler(server, &mqtt_save_uri);
        httpd_register_uri_handler(server, &mqtt_status_uri);
    httpd_register_uri_handler(server, &mqtt_test_uri);
    httpd_register_uri_handler(server, &mqtt_clear_uri);
        httpd_register_uri_handler(server, &as3935_save_uri);
        httpd_register_uri_handler(server, &as3935_status_uri);
        httpd_register_uri_handler(server, &as3935_pins_save_uri);
        httpd_register_uri_handler(server, &as3935_pins_status_uri);
    httpd_register_uri_handler(server, &as3935_params_uri);
    httpd_register_uri_handler(server, &as3935_calibrate_start_uri);
    httpd_register_uri_handler(server, &as3935_calibrate_status_uri);
    httpd_register_uri_handler(server, &as3935_calibrate_cancel_uri);
    httpd_register_uri_handler(server, &as3935_calibrate_apply_uri);
        httpd_register_uri_handler(server, &as3935_reg_uri);
        httpd_register_uri_handler(server, &sse_uri);
        httpd_register_uri_handler(server, &index_uri);
        // register wildcard redirect for captive portal UX
        httpd_register_uri_handler(server, &captive_redirect_uri);
    }

    // init SSE broadcaster
    events_init();

    ESP_LOGI(TAG, "AS3935 Lightning Monitor started");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
