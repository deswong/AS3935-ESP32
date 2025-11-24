#pragma once
#include "esp_http_server.h"

void events_init(void);
void events_broadcast(const char *event, const char *data);
extern httpd_uri_t sse_uri;
