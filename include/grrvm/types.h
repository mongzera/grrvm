#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

#ifndef VM_WORD_SIZE
    /* Common architecture check for 32-bit vs 16-bit targets */
    #if defined(__X86__) || defined(__arm__) || defined(__aarch64__) || \
        defined(__x86_64__) || defined(__riscv) || defined(ESP32) || \
        (defined(__INT_MAX__) && __INT_MAX__ > 32767)
        #define VM_WORD_SIZE 32
        #define EPSILON 0x1.0p-23f
    #else
        #define VM_WORD_SIZE 16
        #if defined(__FLT16_MANT_DIG__)
            #define EPSILON 0x1.0p-10f16
        #else
            /* Fallback to 32-bit float literal if 16-bit type isn't native */
            #define EPSILON 0x1.0p-10f
        #endif
    #endif
#endif

/* Standard byte type for all architectures */
typedef uint8_t byte;

#if (VM_WORD_SIZE == 32)

    /* --- 32-BIT ARCHITECTURE PROFILE --- */
    typedef uint32_t word;    /* Native VM register width: 32 bits */
    typedef uint16_t half;    /* Half-word: 16 bits */

    typedef int32_t  g_int;
    typedef uint32_t g_uint;
    typedef float    g_float; /* IEEE 754 float */

#elif (VM_WORD_SIZE == 16)

    /* --- 16-BIT ARCHITECTURE PROFILE --- */
    typedef uint16_t word;    /* Native VM register width: 16 bits */
    typedef uint8_t  half;    /* Half-word: 8 bits */

    typedef int16_t  g_int;
    typedef uint16_t g_uint;
    /* Note: Many 16-bit microcontrollers lack hardware floating-point units.
       g_float can either use standard float (often emulated in software)
       or be omitted/customized for fixed-point math. */
    typedef float    g_float;

#else
    #error "Unsupported VM_WORD_SIZE. Please specify -DVM_WORD_SIZE=16 or -DVM_WORD_SIZE=32."
#endif

/* Generic primitive aliases */
typedef char g_char;
typedef byte g_byte;

#endif /* TYPES_H */
