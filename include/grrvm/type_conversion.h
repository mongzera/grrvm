#ifndef TYPE_CONVERSION_H
#define TYPE_CONVERSION_H

#include "grrvm/types.h"
#include "grrvm/vm.h"

static inline g_f32 prim_to_float(prim_val val) {
    switch (get_prim_type(val)) {
        case TYPE_I8:           return (g_f32)(g_i8)(g_u8)val.data;
        case TYPE_U8:           return (g_f32)(g_u8)val.data;
        case TYPE_I16:          return (g_f32)(g_i16)(g_u16)val.data;
        case TYPE_U16:          return (g_f32)(g_u16)val.data;
        case TYPE_I32:          return (g_f32)(g_i32)val.data;
        case TYPE_U32:          return (g_f32)(g_u32)val.data;
        case TYPE_FLOAT:        {
            g_f32 f;
            uint32_t u = (uint32_t)val.data;
            __builtin_memcpy(&f, &u, sizeof(f));
            return f;
        }
        default:                return (g_f32)0.0f;
    }
}

static inline g_uint prim_to_uint(prim_val val) {
    switch (get_prim_type(val)) {

        case TYPE_I8:    return (g_uint)(g_i32)(g_i8)(g_u8)val.data;
        case TYPE_U8:    return (g_uint)(g_u8)val.data;
        case TYPE_I16:   return (g_uint)(g_i32)(g_i16)(g_u16)val.data;
        case TYPE_U16:   return (g_uint)(g_u16)val.data;
        case TYPE_I32:   return (g_uint)(g_i32)val.data;
        case TYPE_U32:   return (g_uint)(g_u32)val.data;
        case TYPE_FLOAT: {
            g_f32 f;
            uint32_t u = (uint32_t)val.data;
            __builtin_memcpy(&f, &u, sizeof(f));
            return (g_uint)f;
        }

        default:         return (g_uint)0;
    }
}

static inline g_int prim_to_int(prim_val val) {
    switch (get_prim_type(val)) {

        case TYPE_I8:    return (g_int)(g_i32)(g_i8)(g_u8)val.data;
        case TYPE_U8:    return (g_int)(g_u8)val.data;
        case TYPE_I16:   return (g_int)(g_i32)(g_i16)(g_u16)val.data;
        case TYPE_U16:   return (g_int)(g_u16)val.data;
        case TYPE_I32:   return (g_int)(g_i32)val.data;
        case TYPE_U32:   return (g_int)(g_u32)val.data;
        case TYPE_FLOAT: {
            g_f32 f;
            uint32_t u = (uint32_t)val.data;
            __builtin_memcpy(&f, &u, sizeof(f));
            return (g_int)f;
        }

        default:         return (g_int)0;
    }
}

#endif
