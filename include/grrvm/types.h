#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

/* Single-precision IEEE 754 float epsilon (2^-23) */
#define EPSILON 0x1.0p-23f

/* Fixed-width primitive types */
typedef uint8_t  g_u8;
typedef uint16_t g_u16;
typedef uint32_t g_u32;

typedef int8_t   g_i8;
typedef int16_t  g_i16;
typedef int32_t  g_i32;

typedef float    g_f32;

/* --- UNIFIED 32-BIT VM ENGINE PROFILE --- */
typedef uint32_t word;   /* Native VM register width: 32 bits */
typedef uint16_t half;   /* Half-word: 16 bits */

typedef uint32_t g_uint; /* Guaranteed 32-bit unsigned word */
typedef int32_t  g_int;  /* Guaranteed 32-bit signed word */

typedef uint8_t  byte;

#endif /* TYPES_H */
