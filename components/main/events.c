#include "events.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_http_server.h"

static const char *TAG = "events";

typedef struct sse_client {
    httpd_req_t *req;
    struct sse_client *next;
} sse_client_t;

static sse_client_t *clients = NULL;
static SemaphoreHandle_t clients_mutex;

void events_init(void)
{
    if (!clients_mutex) clients_mutex = xSemaphoreCreateMutex();
    ESP_LOGI(TAG, "events initialized");
}

static void add_client(sse_client_t *c)
{
    xSemaphoreTake(clients_mutex, portMAX_DELAY);
    c->next = clients;
    clients = c;
    xSemaphoreGive(clients_mutex);
}

static void remove_client(sse_client_t *c)
{
    xSemaphoreTake(clients_mutex, portMAX_DELAY);
    sse_client_t **p = &clients;
    while (*p) {
        if (*p == c) { *p = c->next; break; }
        p = &(*p)->next;
    }
    xSemaphoreGive(clients_mutex);
}

void events_broadcast(const char *event, const char *data)
{
    xSemaphoreTake(clients_mutex, portMAX_DELAY);
    sse_client_t *it = clients;
    char buf[1024];
    int offset = 0;
    if (event && event[0]) offset += snprintf(buf + offset, sizeof(buf) - offset, "event: %s\n", event);
    if (data) offset += snprintf(buf + offset, sizeof(buf) - offset, "data: %s\n\n", data);
    else offset += snprintf(buf + offset, sizeof(buf) - offset, "data: \n\n");
    while (it) {
        // best-effort send as chunk; ignore errors per-client
        httpd_resp_sendstr_chunk(it->req, buf);
        it = it->next;
    }
    xSemaphoreGive(clients_mutex);
}

static esp_err_t sse_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_sendstr_chunk(req, "retry: 10000\n\n");
    sse_client_t *client = calloc(1, sizeof(sse_client_t));
    if (!client) return ESP_ERR_NO_MEM;
    client->req = req;
    client->next = NULL;
    add_client(client);
    // keep connection open; send periodic keepalives
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        esp_err_t r = httpd_resp_sendstr_chunk(req, ": keepalive\n\n");
        if (r != ESP_OK) {
            // client disconnected or send failed
            remove_client(client);
            free(client);
            return r;
        }
    }
}

httpd_uri_t sse_uri = {
    .uri = "/api/events/stream",
    .method = HTTP_GET,
    .handler = sse_handler,
    .user_ctx = NULL
};
