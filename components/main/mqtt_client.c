#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "app_mqtt.h"
#include "esp_http_server.h"
#include <mqtt_client.h> // IDF esp-mqtt header
#include "esp_log.h"
#include "settings.h"
#include "cJSON.h"
#include "http_helpers.h"

static const char *TAG = "mqtt";
static esp_mqtt_client_handle_t client = NULL;

static esp_err_t mqtt_stop_internal(void)
{
    if (client) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = NULL;
    }
    return ESP_OK;
}

esp_err_t mqtt_init(const mqtt_config_t *cfg)
{
    if (!cfg) return ESP_ERR_INVALID_ARG;
    // stop previous client if exists
    mqtt_stop_internal();
    esp_mqtt_client_config_t mqtt_cfg = {0};
    // set broker URI and transport
    mqtt_cfg.broker.address.uri = cfg->uri;
    mqtt_cfg.broker.address.transport = cfg->use_tls ? MQTT_TRANSPORT_OVER_SSL : MQTT_TRANSPORT_OVER_TCP;
    // client credentials
    mqtt_cfg.credentials.client_id = cfg->client_id;
    mqtt_cfg.credentials.username = cfg->username;
    mqtt_cfg.credentials.authentication.password = cfg->password;
    // CA cert for broker verification (PEM, NULL-terminated -> set len=0)
    if (cfg->ca_cert) {
        mqtt_cfg.broker.verification.certificate = cfg->ca_cert;
        mqtt_cfg.broker.verification.certificate_len = 0;
    }
    client = esp_mqtt_client_init(&mqtt_cfg);
    if (!client) return ESP_FAIL;
    esp_err_t err = esp_mqtt_client_start(client);
    if (err == ESP_OK) ESP_LOGI(TAG, "MQTT started to %s (tls=%d)", cfg->uri, cfg->use_tls);
    return err;
}

esp_err_t mqtt_publish(const char *topic, const char *payload)
{
    if (!client) return ESP_ERR_INVALID_STATE;
    int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
    if (msg_id < 0) return ESP_FAIL;
    ESP_LOGI(TAG, "Published msg id %d to %s", msg_id, topic);
    return ESP_OK;
}

void mqtt_stop(void)
{
    mqtt_stop_internal();
}

// HTTP handlers for saving and checking MQTT settings
esp_err_t mqtt_save_handler(httpd_req_t *req)
{
    int content_len = req->content_len;
    if (content_len <= 0 || content_len > 1024) { http_helpers_send_400(req); return ESP_FAIL; }
    char *buf = malloc(content_len + 1);
    if (!buf) { http_helpers_send_500(req); return ESP_ERR_NO_MEM; }
    int ret = httpd_req_recv(req, buf, content_len);
    if (ret <= 0) { free(buf); http_helpers_send_500(req); return ESP_FAIL; }
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) { http_helpers_send_400(req); return ESP_FAIL; }
    const cJSON *uri = cJSON_GetObjectItemCaseSensitive(root, "uri");
    const cJSON *use_tls = cJSON_GetObjectItemCaseSensitive(root, "use_tls");
    const cJSON *username = cJSON_GetObjectItemCaseSensitive(root, "username");
    const cJSON *password = cJSON_GetObjectItemCaseSensitive(root, "password");
    const cJSON *ca_cert = cJSON_GetObjectItemCaseSensitive(root, "ca_cert");
    const cJSON *topic = cJSON_GetObjectItemCaseSensitive(root, "topic");
    if (!cJSON_IsString(uri) || (uri->valuestring == NULL)) { cJSON_Delete(root); http_helpers_send_400(req); return ESP_FAIL; }
    settings_save_str("mqtt", "uri", uri->valuestring);
    if (cJSON_IsString(username) && username->valuestring) settings_save_str("mqtt", "username", username->valuestring);
    if (cJSON_IsString(password) && password->valuestring) settings_save_str("mqtt", "password", password->valuestring);
    if (cJSON_IsString(ca_cert) && ca_cert->valuestring) settings_save_str("mqtt", "ca_cert", ca_cert->valuestring);
    if (cJSON_IsBool(use_tls)) {
        settings_save_str("mqtt", "tls", cJSON_IsTrue(use_tls) ? "1" : "0");
    }
    if (cJSON_IsString(topic) && topic->valuestring) {
        settings_save_str("mqtt", "topic", topic->valuestring);
    }
    cJSON_Delete(root);

    // apply immediately
    char saved_uri[256] = {0};
    settings_load_str("mqtt", "uri", saved_uri, sizeof(saved_uri));
    char tls_str[8] = {0};
    settings_load_str("mqtt", "tls", tls_str, sizeof(tls_str));
    char username_saved[128] = {0};
    settings_load_str("mqtt", "username", username_saved, sizeof(username_saved));
    char password_saved[128] = {0};
    settings_load_str("mqtt", "password", password_saved, sizeof(password_saved));
    char ca_saved[2048] = {0};
    settings_load_str("mqtt", "ca_cert", ca_saved, sizeof(ca_saved));
    char topic_saved[256] = {0};
    settings_load_str("mqtt", "topic", topic_saved, sizeof(topic_saved));
    mqtt_config_t cfg = { .uri = saved_uri, .use_tls = (tls_str[0] == '1'), .client_id = "as3935_esp32", .username = username_saved[0]?username_saved:NULL, .password = password_saved[0]?password_saved:NULL, .ca_cert = ca_saved[0]?ca_saved:NULL };
    mqtt_init(&cfg);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}\n");
    return ESP_OK;
}

esp_err_t mqtt_status_handler(httpd_req_t *req)
{
    char saved_uri[256] = {0};
    settings_load_str("mqtt", "uri", saved_uri, sizeof(saved_uri));
    char tls_str[8] = {0};
    settings_load_str("mqtt", "tls", tls_str, sizeof(tls_str));
    char topic_saved[256] = {0};
    settings_load_str("mqtt", "topic", topic_saved, sizeof(topic_saved));
    char username_saved[128] = {0};
    settings_load_str("mqtt", "username", username_saved, sizeof(username_saved));
    char password_saved[128] = {0};
    settings_load_str("mqtt", "password", password_saved, sizeof(password_saved));
    char ca_saved[256] = {0};
    settings_load_str("mqtt", "ca_cert", ca_saved, sizeof(ca_saved));
    char buf[1024];
    int password_set = password_saved[0] ? 1 : 0;
    const char *password_mask = password_set ? "********" : "";
    snprintf(buf, sizeof(buf), "{\"configured\":%s, \"uri\":\"%s\", \"use_tls\":%s, \"topic\":\"%s\", \"username\":\"%s\", \"has_ca\":%s, \"password_set\":%s, \"password_masked\":\"%s\"}", saved_uri[0]?"true":"false", saved_uri, tls_str[0]=='1' ? "true" : "false", topic_saved, username_saved, ca_saved[0]?"true":"false", password_set?"true":"false", password_mask);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

// Test publish: publish a small test message to the configured topic
esp_err_t mqtt_test_publish_handler(httpd_req_t *req)
{
    // read topic from NVS
    char topic_saved[256] = {0};
    if (settings_load_str("mqtt", "topic", topic_saved, sizeof(topic_saved)) != ESP_OK || topic_saved[0]==0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"no_topic\"}\n");
        return ESP_FAIL;
    }
    const char *payload = "{\"test\":true, \"source\":\"device\"}";
    esp_err_t err = mqtt_publish(topic_saved, payload);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"publish_failed\"}\n");
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}\n");
    return ESP_OK;
}

// Clear stored credentials (username/password/ca_cert)
esp_err_t mqtt_clear_credentials_handler(httpd_req_t *req)
{
    settings_erase_key("mqtt", "username");
    settings_erase_key("mqtt", "password");
    settings_erase_key("mqtt", "ca_cert");
    // re-init MQTT without credentials
    char saved_uri[256] = {0};
    settings_load_str("mqtt", "uri", saved_uri, sizeof(saved_uri));
    char tls_str[8] = {0};
    settings_load_str("mqtt", "tls", tls_str, sizeof(tls_str));
    char topic_saved[256] = {0};
    settings_load_str("mqtt", "topic", topic_saved, sizeof(topic_saved));
    mqtt_config_t cfg = { .uri = saved_uri, .use_tls = (tls_str[0] == '1'), .client_id = "as3935_esp32", .username = NULL, .password = NULL, .ca_cert = NULL };
    mqtt_init(&cfg);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}\n");
    return ESP_OK;
}
