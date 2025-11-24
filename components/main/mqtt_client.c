#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "app_mqtt.h"
#include "esp_http_server.h"
#include <mqtt_client.h>  // use system header
#include "esp_log.h"
#include "settings.h"
#include "cJSON.h"
#include "http_helpers.h"
#include "esp_idf_version.h"
#include "freertos/task.h"

static const char *TAG = "mqtt";
static esp_mqtt_client_handle_t client = NULL;
static bool mqtt_connected = false;

// Task handle for availability publisher
static TaskHandle_t availability_task = NULL;

// Task to publish availability after connection is settled
static void publish_availability_task(void *pvParameters) {
	// Wait a bit for connection to fully settle
	vTaskDelay(pdMS_TO_TICKS(500));
	
	if (client && mqtt_connected) {
		// Load configured availability topic from NVS
		char availability_topic[256] = "as3935/availability";  // Default
		settings_load_str("mqtt", "availability_topic", availability_topic, sizeof(availability_topic));
		
		int msg_id = esp_mqtt_client_publish(client, availability_topic, "online", 0, 1, 1);
		if (msg_id >= 0) {
			ESP_LOGI(TAG, "✓ Published 'online' to %s (msg_id=%d)", availability_topic, msg_id);
		} else {
			ESP_LOGW(TAG, "Failed to publish 'online' to %s (msg_id=%d)", availability_topic, msg_id);
		}
	}
	
	// Clean up task
	availability_task = NULL;
	vTaskDelete(NULL);
}

static esp_err_t mqtt_stop_internal(void)
{
	if (client) {
		esp_mqtt_client_stop(client);
		esp_mqtt_client_destroy(client);
		client = NULL;
		mqtt_connected = false;
	}
	return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
	esp_mqtt_event_handle_t event = event_data;
	
	ESP_LOGD(TAG, "MQTT event received: event_id=%d", event->event_id);
	
	switch (event->event_id) {
	case MQTT_EVENT_CONNECTED:
		mqtt_connected = true;
		ESP_LOGI(TAG, "MQTT connected - publishing availability status");
		// Spawn a task to publish "online" after connection settles (500ms delay)
		if (availability_task == NULL) {
			xTaskCreate(publish_availability_task, "mqtt_avail", 2048, NULL, 5, &availability_task);
		}
		break;
	case MQTT_EVENT_DISCONNECTED:
		mqtt_connected = false;
		ESP_LOGI(TAG, "MQTT disconnected - LWT will publish 'offline' to as3935/availability");
		break;
		case MQTT_EVENT_ERROR:
			mqtt_connected = false;
			ESP_LOGW(TAG, "MQTT error: error_type=%d", event->error_handle->error_type);
			if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
				ESP_LOGW(TAG, "TCP connection failed - check IP/port and firewall");
			} else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
				ESP_LOGW(TAG, "Broker rejected connection - check credentials");
			}
			break;
		case MQTT_EVENT_BEFORE_CONNECT:
			ESP_LOGI(TAG, "MQTT attempting to connect...");
			break;
		default:
			ESP_LOGD(TAG, "MQTT event (unhandled): %d", event->event_id);
			break;
	}
}

esp_err_t mqtt_init(const mqtt_config_t *cfg)
{
	if (!cfg) return ESP_ERR_INVALID_ARG;

	// stop previous client if exists
	mqtt_stop_internal();
	
	// Log MQTT configuration for debugging
	ESP_LOGI(TAG, "MQTT init: uri=%s, tls=%d, client_id=%s, username=%s", 
		cfg->uri ? cfg->uri : "NULL",
		cfg->use_tls,
		cfg->client_id ? cfg->client_id : "NULL",
		cfg->username ? cfg->username : "NULL");
	
	esp_mqtt_client_config_t mqtt_cfg = {0};
	// set broker URI - let URI scheme determine transport (mqtt:// vs mqtts://)
	mqtt_cfg.broker.address.uri = cfg->uri;
	// DO NOT set transport explicitly - it will be overridden by URI scheme
	// mqtt_cfg.broker.address.transport = cfg->use_tls ? MQTT_TRANSPORT_OVER_SSL : MQTT_TRANSPORT_OVER_TCP;
	// client credentials
	mqtt_cfg.credentials.client_id = cfg->client_id;
	mqtt_cfg.credentials.username = cfg->username;
	mqtt_cfg.credentials.authentication.password = cfg->password;
	// CA cert for broker verification (PEM, NULL-terminated -> set len=0)
	if (cfg->ca_cert) {
		mqtt_cfg.broker.verification.certificate = cfg->ca_cert;
		mqtt_cfg.broker.verification.certificate_len = 0;
	}
	
	// Load availability topic from NVS for Last Will Testament
	static char availability_topic_buffer[256] = "as3935/availability";
	settings_load_str("mqtt", "availability_topic", availability_topic_buffer, sizeof(availability_topic_buffer));
	
	// Set Last Will Testament (LWT) for availability - published if connection is lost
	mqtt_cfg.session.last_will.topic = availability_topic_buffer;
	mqtt_cfg.session.last_will.msg = "offline";
	mqtt_cfg.session.last_will.msg_len = strlen("offline");
	mqtt_cfg.session.last_will.qos = 1;
	mqtt_cfg.session.last_will.retain = true;
	
	ESP_LOGI(TAG, "LWT configured: topic=%s, message=offline", availability_topic_buffer);
	
	client = esp_mqtt_client_init(&mqtt_cfg);
	if (!client) {
		ESP_LOGE(TAG, "MQTT client init failed - memory or config error");
		return ESP_FAIL;
	}
	ESP_LOGI(TAG, "MQTT client created successfully");
	
	// Register event handler
	esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
	
	esp_err_t err = esp_mqtt_client_start(client);
	if (err == ESP_OK) ESP_LOGI(TAG, "MQTT started to %s (tls=%d)", cfg->uri, cfg->use_tls);
	return err;
}

esp_err_t mqtt_publish(const char *topic, const char *payload)
{
	if (!client) {
		ESP_LOGW(TAG, "[MQTT-PUB] MQTT client not initialized");
		return ESP_ERR_INVALID_STATE;
	}
	
	ESP_LOGI(TAG, "[MQTT-PUB] Attempting publish: connected=%d, topic='%s', payload='%s'", 
	         mqtt_connected, topic, payload);
	
	int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
	if (msg_id < 0) {
		ESP_LOGW(TAG, "[MQTT-PUB] Failed: msg_id=%d (client may not be connected yet, connected=%d)", 
		         msg_id, mqtt_connected);
		return ESP_FAIL;
	}
	
	ESP_LOGI(TAG, "[MQTT-PUB] Success: msg_id %d published to topic '%s'", msg_id, topic);
	return ESP_OK;
}

bool mqtt_is_connected(void)
{
	return mqtt_connected;
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

	const cJSON *root = cJSON_Parse(buf);
	free(buf);
	if (!root) { http_helpers_send_400(req); return ESP_FAIL; }
	const cJSON *uri = cJSON_GetObjectItemCaseSensitive(root, "uri");
	const cJSON *use_tls = cJSON_GetObjectItemCaseSensitive(root, "use_tls");
	const cJSON *username = cJSON_GetObjectItemCaseSensitive(root, "username");
	const cJSON *password = cJSON_GetObjectItemCaseSensitive(root, "password");
	const cJSON *ca_cert = cJSON_GetObjectItemCaseSensitive(root, "ca_cert");
	const cJSON *topic = cJSON_GetObjectItemCaseSensitive(root, "topic");
	const cJSON *availability_topic = cJSON_GetObjectItemCaseSensitive(root, "availability_topic");
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
	} else {
		// Set default topic if not provided
		settings_save_str("mqtt", "topic", "as3935/lightning");
	}
	if (cJSON_IsString(availability_topic) && availability_topic->valuestring) {
		settings_save_str("mqtt", "availability_topic", availability_topic->valuestring);
	} else {
		// Set default availability topic if not provided
		settings_save_str("mqtt", "availability_topic", "as3935/availability");
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

// Helper function to escape JSON string values
static void json_escape_string(const char *src, char *dst, size_t dst_len) {
	if (!src) src = "";
	size_t j = 0;
	for (size_t i = 0; src[i] && j < dst_len - 2; i++) {
		char c = src[i];
		if (c == '"' || c == '\\') {
			if (j + 1 >= dst_len - 2) break;
			dst[j++] = '\\';
			dst[j++] = c;
		} else if (c == '\n') {
			if (j + 1 >= dst_len - 2) break;
			dst[j++] = '\\';
			dst[j++] = 'n';
		} else if (c == '\r') {
			if (j + 1 >= dst_len - 2) break;
			dst[j++] = '\\';
			dst[j++] = 'r';
		} else if (c == '\t') {
			if (j + 1 >= dst_len - 2) break;
			dst[j++] = '\\';
			dst[j++] = 't';
		} else {
			dst[j++] = c;
		}
	}
	dst[j] = '\0';
}

esp_err_t mqtt_status_handler(httpd_req_t *req)
{
	char saved_uri[256] = {0};
	settings_load_str("mqtt", "uri", saved_uri, sizeof(saved_uri));
	char tls_str[8] = {0};
	settings_load_str("mqtt", "tls", tls_str, sizeof(tls_str));
	char topic_saved[256] = {0};
	settings_load_str("mqtt", "topic", topic_saved, sizeof(topic_saved));
	char availability_topic_saved[256] = {0};
	settings_load_str("mqtt", "availability_topic", availability_topic_saved, sizeof(availability_topic_saved));
	char username_saved[128] = {0};
	settings_load_str("mqtt", "username", username_saved, sizeof(username_saved));
	char password_saved[128] = {0};
	settings_load_str("mqtt", "password", password_saved, sizeof(password_saved));
	char ca_saved[256] = {0};
	settings_load_str("mqtt", "ca_cert", ca_saved, sizeof(ca_saved));
	
	bool connected = mqtt_is_connected();
	
	// Escape strings for JSON
	char escaped_uri[512] = {0};
	char escaped_topic[512] = {0};
	char escaped_availability_topic[512] = {0};
	char escaped_username[256] = {0};
	json_escape_string(saved_uri, escaped_uri, sizeof(escaped_uri));
	json_escape_string(topic_saved, escaped_topic, sizeof(escaped_topic));
	json_escape_string(availability_topic_saved, escaped_availability_topic, sizeof(escaped_availability_topic));
	json_escape_string(username_saved, escaped_username, sizeof(escaped_username));
	
	char buf[2048];
	int password_set = password_saved[0] ? 1 : 0;
	const char *password_mask = password_set ? "********" : "";
	snprintf(buf, sizeof(buf), "{\"configured\":%s,\"uri\":\"%s\",\"use_tls\":%s,\"topic\":\"%s\",\"availability_topic\":\"%s\",\"username\":\"%s\",\"has_ca\":%s,\"password_set\":%s,\"password_masked\":\"%s\",\"connected\":%s}", 
		saved_uri[0]?"true":"false", escaped_uri, tls_str[0]=='1' ? "true" : "false", escaped_topic, escaped_availability_topic, escaped_username, ca_saved[0]?"true":"false", password_set?"true":"false", password_mask, connected?"true":"false");
	
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
		httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"no_topic_configured\"}\n");
		return ESP_FAIL;
	}
	
	// Check if MQTT URI is configured
	char uri_saved[256] = {0};
	if (settings_load_str("mqtt", "uri", uri_saved, sizeof(uri_saved)) != ESP_OK || uri_saved[0]==0) {
		httpd_resp_set_status(req, "400 Bad Request");
		httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"no_mqtt_broker_configured\"}\n");
		return ESP_FAIL;
	}
	
	const char *payload = "{\"test\":true, \"source\":\"device\"}";
	esp_err_t err = mqtt_publish(topic_saved, payload);
	if (err != ESP_OK) {
		httpd_resp_set_status(req, "503 Service Unavailable");
		httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"mqtt_not_connected\",\"message\":\"MQTT client is not connected yet. Please wait a moment and try again.\"}\n");
		return ESP_FAIL;
	}
	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr(req, "{\"ok\":true,\"message\":\"Test message published successfully\"}\n");
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
