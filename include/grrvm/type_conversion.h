#ifndef GRRVM_TYPE_CONVERSION_H
#define GRRVM_TYPE_CONVERSION_H

#include "grrvm/types.h"
#include "grrvm/prim_val.h"

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
        case TYPE_REFERENCE:
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
        case TYPE_REFERENCE:
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

static inline prim_val convert_prim(prim_val val, prim_type target_type) {
    prim_type current_type = get_prim_type(val);
    if (current_type == target_type) {
        return val;
    }

    prim_val result;
    prim_state state = get_prim_state(val);
    result.metadata = pack_meta(state, target_type);

    switch (target_type) {
        case TYPE_I8: {
            g_int i = prim_to_int(val);
            result.data = (uint32_t)(g_i32)(g_i8)i;
            break;
        }
        case TYPE_U8: {
            g_uint u = prim_to_uint(val);
            result.data = (uint32_t)(g_u8)u;
            break;
        }
        case TYPE_I16: {
            g_int i = prim_to_int(val);
            result.data = (uint32_t)(g_i32)(g_i16)i;
            break;
        }
        case TYPE_U16: {
            g_uint u = prim_to_uint(val);
            result.data = (uint32_t)(g_u16)u;
            break;
        }
        case TYPE_I32: {
            g_int i = prim_to_int(val);
            result.data = (uint32_t)i;
            break;
        }
        case TYPE_U32:
        case TYPE_REFERENCE: {
            g_uint u = prim_to_uint(val);
            result.data = (uint32_t)u;
            break;
        }
        case TYPE_FLOAT: {
            g_f32 f = prim_to_float(val);
            uint32_t u;
            __builtin_memcpy(&u, &f, sizeof(u));
            result.data = u;
            break;
        }
        default: {
            result.data = 0;
            result.metadata = pack_meta(state, TYPE_NULL);
            break;
        }
    }

    return result;
}

#endif /* GRRVM_TYPE_CONVERSION_H */
