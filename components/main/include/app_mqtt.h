#pragma once
#include <stdbool.h>
#include <esp_err.h>

// Forward-declare httpd request type to avoid ordering issues
typedef struct httpd_req httpd_req_t;

typedef struct {
    const char *uri; // e.g. "mqtt://host:1883" or "mqtts://..."
    bool use_tls;
    const char *client_id;
    const char *username;
    const char *password;
    const char *ca_cert;
} mqtt_config_t;

esp_err_t mqtt_init(const mqtt_config_t *cfg);
esp_err_t mqtt_publish(const char *topic, const char *payload);
void mqtt_stop(void);

/* HTTP handlers */
esp_err_t mqtt_save_handler(httpd_req_t *req);
esp_err_t mqtt_status_handler(httpd_req_t *req);
esp_err_t mqtt_test_publish_handler(httpd_req_t *req);
esp_err_t mqtt_clear_credentials_handler(httpd_req_t *req);
