/**
 * @file type_utils.h
 * @brief Type utilities header for ESP components
 * 
 * Minimal type utilities for ESP components compatibility
 */

#ifndef __TYPE_UTILS_H__
#define __TYPE_UTILS_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Type compatibility macros */
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float float32;
typedef double float64;

/* Buffer type definitions */
typedef uint8_t bit8_uint8_buffer_t[1];
typedef uint8_t bit16_uint8_buffer_t[2];
typedef uint8_t bit24_uint8_buffer_t[3];

#define BIT8_UINT8_BUFFER_SIZE   1
#define BIT16_UINT8_BUFFER_SIZE  2
#define BIT24_UINT8_BUFFER_SIZE  3

/* CPU definitions for compatibility */
#ifndef APP_CPU_NUM
#define APP_CPU_NUM 1
#endif

#ifndef PRO_CPU_NUM
#define PRO_CPU_NUM 0
#endif

#ifdef __cplusplus
}
#endif

#endif // __TYPE_UTILS_H__
