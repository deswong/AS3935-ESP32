/* Lightweight cJSON shim for this project - minimal API subset */

#ifndef CJSON_H
#define CJSON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cJSON {
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child; // for arrays/objects
    int type;
    char *valuestring;
    int valueint;
    double valuedouble;
    char *string; // object key
} cJSON;

/* types */
#define cJSON_False 0
#define cJSON_True 1
#define cJSON_NULL 2
#define cJSON_Number 3
#define cJSON_String 4
#define cJSON_Array 5
#define cJSON_Object 6

cJSON *cJSON_Parse(const char *value);
void cJSON_Delete(cJSON *item);
int cJSON_IsNumber(const cJSON *item);
int cJSON_IsString(const cJSON *item);
int cJSON_IsBool(const cJSON *item);
int cJSON_IsTrue(const cJSON *item);
int cJSON_IsFalse(const cJSON *item);
const cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON * const object, const char * const string);

#define cJSON_ArrayForEach(element, array) for(element = (array != NULL ? (array)->child : NULL); element != NULL; element = element->next)

#ifdef __cplusplus
}
#endif

#endif /* CJSON_H */
