// Background OTA task supporting HTTP and HTTPS with progress reporting via SSE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ota.h"
#include "esp_log.h"
#include "cJSON.h"
#include "http_helpers.h"
#include "settings.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "events.h"

static const char *TAG = "ota";

typedef struct {
    char *url;
} ota_args_t;

static void ota_task(void *pv)
{
    ota_args_t *args = (ota_args_t *)pv;
    const char *url = args->url;
    char payload[256];
    snprintf(payload, sizeof(payload), "{\"status\":\"start\", \"url\":\"%s\"}", url);
    events_broadcast("ota_progress", payload);

    esp_http_client_config_t config = {
        .url = url,
        .keep_alive_enable = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init http client");
        events_broadcast("ota_progress", "{\"status\":\"failed\", \"reason\":\"client_init\"}");
        goto cleanup;
    }

    if (esp_http_client_open(client, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection");
        events_broadcast("ota_progress", "{\"status\":\"failed\", \"reason\":\"open_failed\"}");
        esp_http_client_cleanup(client);
        goto cleanup;
    }

    int content_length = esp_http_client_fetch_headers(client);
    if (content_length <= 0) {
        ESP_LOGW(TAG, "Content length unknown or zero: %d", content_length);
    }

    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (!update_partition) {
        ESP_LOGE(TAG, "No update partition found");
        events_broadcast("ota_progress", "{\"status\":\"failed\", \"reason\":\"no_partition\"}");
        esp_http_client_cleanup(client);
        goto cleanup;
    }

    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %d", err);
        events_broadcast("ota_progress", "{\"status\":\"failed\", \"reason\":\"ota_begin\"}");
        esp_http_client_cleanup(client);
        goto cleanup;
    }

    char buffer[1024];
    int total_written = 0;
    while (1) {
        int data_read = esp_http_client_read(client, buffer, sizeof(buffer));
        if (data_read < 0) {
            ESP_LOGE(TAG, "HTTP read error");
            events_broadcast("ota_progress", "{\"status\":\"failed\", \"reason\":\"read_error\"}");
            err = ESP_FAIL;
            break;
        } else if (data_read == 0) {
            // EOF
            err = ESP_OK;
            break;
        }
        err = esp_ota_write(ota_handle, (const void *)buffer, data_read);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %d", err);
            events_broadcast("ota_progress", "{\"status\":\"failed\", \"reason\":\"write_error\"}");
            break;
        }
        total_written += data_read;
        if (content_length > 0) {
            int pct = (total_written * 100) / content_length;
            snprintf(payload, sizeof(payload), "{\"status\":\"progress\", \"written\":%d, \"total\":%d, \"percent\":%d}", total_written, content_length, pct);
            events_broadcast("ota_progress", payload);
        } else {
            snprintf(payload, sizeof(payload), "{\"status\":\"progress\", \"written\":%d}", total_written);
            events_broadcast("ota_progress", payload);
        }
    }

    if (err == ESP_OK) {
        err = esp_ota_end(ota_handle);
        if (err == ESP_OK) {
            err = esp_ota_set_boot_partition(update_partition);
            if (err == ESP_OK) {
                snprintf(payload, sizeof(payload), "{\"status\":\"done\", \"written\":%d}", total_written);
                events_broadcast("ota_progress", payload);
                vTaskDelay(pdMS_TO_TICKS(500));
                esp_restart();
            } else {
                ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %d", err);
                events_broadcast("ota_progress", "{\"status\":\"failed\", \"reason\":\"set_boot_failed\"}");
            }
        } else {
            ESP_LOGE(TAG, "esp_ota_end failed: %d", err);
            events_broadcast("ota_progress", "{\"status\":\"failed\", \"reason\":\"ota_end\"}");
        }
    }

    esp_http_client_cleanup(client);

cleanup:
    if (args) {
        if (args->url) free(args->url);
        free(args);
    }
    vTaskDelete(NULL);
}

esp_err_t ota_start_handler(httpd_req_t *req)
{
    int content_len = req->content_len;
    if (content_len <= 0 || content_len > 4096) { http_helpers_send_400(req); return ESP_FAIL; }
    char *buf = malloc(content_len + 1);
    if (!buf) { http_helpers_send_500(req); return ESP_ERR_NO_MEM; }
    int ret = httpd_req_recv(req, buf, content_len);
    if (ret <= 0) { free(buf); http_helpers_send_500(req); return ESP_FAIL; }
    buf[ret] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) { http_helpers_send_400(req); return ESP_FAIL; }
    const cJSON *url = cJSON_GetObjectItemCaseSensitive(root, "url");
    if (!cJSON_IsString(url) || !url->valuestring) { cJSON_Delete(root); http_helpers_send_400(req); return ESP_FAIL; }

    // spawn background OTA task
    ota_args_t *args = calloc(1, sizeof(ota_args_t));
    if (!args) { cJSON_Delete(root); http_helpers_send_500(req); return ESP_ERR_NO_MEM; }
    args->url = strdup(url->valuestring);
    if (!args->url) { free(args); cJSON_Delete(root); http_helpers_send_500(req); return ESP_ERR_NO_MEM; }

    BaseType_t ok = xTaskCreate(ota_task, "ota_task", 8*1024, args, 5, NULL);
    cJSON_Delete(root);
    if (ok != pdPASS) {
        if (args->url) free(args->url);
        free(args);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true, \"started\":true}\n");
    return ESP_OK;
}
