/* Minimal JSON parser shim - supports very small, flat JSON objects used by the project.
   This is not a full JSON parser. It handles objects like {"0x03":150, "key": 5}
   and simple string/number/boolean values. It is intentionally small to avoid
   external dependencies.
*/

#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *strndup_local(const char *s, size_t n) {
    char *p = malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static void free_item(cJSON *it) {
    if (!it) return;
    if (it->valuestring) free(it->valuestring);
    if (it->string) free(it->string);
    free(it);
}

void cJSON_Delete(cJSON *item) {
    if (!item) return;
    cJSON *cur = item->child;
    while (cur) {
        cJSON *n = cur->next;
        free_item(cur);
        cur = n;
    }
    free_item(item);
}

int cJSON_IsNumber(const cJSON *item) { return item && item->type == cJSON_Number; }
int cJSON_IsString(const cJSON *item) { return item && item->type == cJSON_String; }
int cJSON_IsBool(const cJSON *item) { return item && (item->type == cJSON_False || item->type == cJSON_True); }
int cJSON_IsTrue(const cJSON *item) { return item && item->type == cJSON_True; }
int cJSON_IsFalse(const cJSON *item) { return item && item->type == cJSON_False; }

const cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON * const object, const char * const string) {
    if (!object || !string) return NULL;
    cJSON *cur = object->child;
    while (cur) {
        if (cur->string && strcmp(cur->string, string) == 0) return cur;
        cur = cur->next;
    }
    return NULL;
}

// Very small parser: expects a JSON object with string keys and simple values.
// Handles numbers, strings (".."), true, false, null. Skips whitespace.
/* suppress misleading-indentation warnings in this tiny helper across compilers */
static const char *skip_ws(const char *p) {
    size_t i = 0;
    while (p[i] && isspace((unsigned char)p[i])) {
        i++;
    }
    return p + i;
}

static char *parse_string(const char **pp) {
    const char *p = *pp;
    if (*p != '"') return NULL;
    p++;
    const char *start = p;
    while (*p && *p != '"') {
        if (*p == '\\' && *(p+1)) p+=2; else p++;
    }
    if (*p != '"') return NULL;
    size_t len = p - start;
    char *s = strndup_local(start, len);
    *pp = p+1;
    return s;
}

static long parse_number(const char **pp) {
    const char *p = *pp;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    long val = 0;
    while (*p && isdigit((unsigned char)*p)) { val = val*10 + (*p - '0'); p++; }
    *pp = p;
    return neg ? -val : val;
}

cJSON *cJSON_Parse(const char *value) {
    if (!value) return NULL;
    const char *p = value;
    p = skip_ws(p);
    if (*p != '{') return NULL;
    p++;
    cJSON *root = malloc(sizeof(cJSON));
    if (!root) return NULL;
    memset(root, 0, sizeof(cJSON));
    root->type = cJSON_Object;
    cJSON *last = NULL;

    while (1) {
        p = skip_ws(p);
        if (*p == '}') { p++; break; }
        // parse key
        char *key = parse_string(&p);
        if (!key) { cJSON_Delete(root); return NULL; }
        p = skip_ws(p);
        if (*p != ':') { free(key); cJSON_Delete(root); return NULL; }
        p++;
        p = skip_ws(p);
        // parse value
        cJSON *item = malloc(sizeof(cJSON));
        if (!item) { free(key); cJSON_Delete(root); return NULL; }
        memset(item, 0, sizeof(cJSON));
        item->string = key;
        if (*p == '"') {
            item->type = cJSON_String;
            item->valuestring = parse_string(&p);
        } else if (isdigit((unsigned char)*p) || *p=='-' ) {
            item->type = cJSON_Number;
            item->valueint = (int)parse_number(&p);
        } else if (strncmp(p, "true", 4) == 0) {
            item->type = cJSON_True; p += 4;
        } else if (strncmp(p, "false", 5) == 0) {
            item->type = cJSON_False; p += 5;
        } else if (strncmp(p, "null", 4) == 0) {
            item->type = cJSON_NULL; p += 4;
        } else {
            // unsupported
            free(item->string); free(item); cJSON_Delete(root); return NULL;
        }
        // append to list
        item->next = NULL; item->prev = last; item->child = NULL;
        if (last) last->next = item; else root->child = item;
        last = item;
        p = skip_ws(p);
        if (*p == ',') { p++; continue; }
        if (*p == '}') { p++; break; }
        // otherwise error
        cJSON_Delete(root); return NULL;
    }
    return root;
}
