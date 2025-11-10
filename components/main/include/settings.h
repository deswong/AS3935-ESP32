#pragma once
#include <esp_err.h>
#include <stddef.h>

esp_err_t settings_init(void);
esp_err_t settings_save_str(const char *ns, const char *key, const char *value);
esp_err_t settings_load_str(const char *ns, const char *key, char *out, size_t len);
esp_err_t settings_erase_key(const char *ns, const char *key);
