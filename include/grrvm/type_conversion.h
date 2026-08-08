#ifndef TYPE_CONVERSION_H
#define TYPE_CONVERSION_H

#include "grrvm/types.h"
#include "grrvm/vm.h"
#include <string.h>
static inline g_float prim_to_float(prim_val val) {
    switch (get_prim_type(val)) {
        case TYPE_BYTE:  return (g_float)(uint8_t)val.data;
        case TYPE_CHAR:  return (g_float)(int8_t)val.data;
        case TYPE_UINT:  return (g_float)(uint32_t)val.data;
        case TYPE_INT:   return (g_float)(int32_t)val.data;
        case TYPE_FLOAT: {
            g_float f;
            uint32_t raw = (uint32_t)val.data;
            memcpy(&f, &raw, sizeof(g_float));
            return f;
        }
        default: return 0.0f;
    }
}

static inline g_uint prim_to_uint(prim_val val) {
    switch (get_prim_type(val)) {
        case TYPE_BYTE:  return (g_uint)(uint8_t)val.data;
        case TYPE_CHAR:  return (g_uint)(int8_t)val.data;
        case TYPE_UINT:  return (g_uint)(uint32_t)val.data;
        case TYPE_INT:   return (g_uint)(int32_t)val.data;
        case TYPE_FLOAT: {
            g_int f;
            uint32_t raw = (uint32_t)val.data;
            memcpy(&f, &raw, sizeof(g_int));
            return f;
        }
        default: return 0;
    }
}

#endif
