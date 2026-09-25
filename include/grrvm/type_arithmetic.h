#ifndef TYPE_ARITHMETIC_H
#define TYPE_ARITHMETIC_H

// --- TYPE-SAFE ARITHMETIC PRIMITIVES ---

#include "grrvm/type_checks.h"
#include "grrvm/type_conversion.h"
#include "grrvm/type_promotion.h"
#include "grrvm/types.h"
#include "grrvm/vm.h"
#include <math.h>

/* Helper to convert float bit pattern to word */
static inline word float_to_prim_data(g_f32 f) {
    uint32_t u;
    __builtin_memcpy(&u, &f, sizeof(f));
    return (word)u;
}

/* Helper to mask integer results into their target type's raw payload format */
static inline word format_prim_data(g_u32 res, prim_type type) {
    switch (type) {
        case TYPE_I8:
        case TYPE_U8:  return (word)(g_u8)res;
        case TYPE_I16:
        case TYPE_U16: return (word)(g_u16)res;
        default:       return (word)res; // TYPE_I32, TYPE_U32
    }
}

static inline int type_safe_add(prim_val a, prim_val b, prim_val *out_res) {
    prim_type target_type;
    if (!promote_types(get_prim_type(a), get_prim_type(b), &target_type)) return 0;

    word data;
    if (target_type == TYPE_FLOAT) {
        data = float_to_prim_data(prim_to_float(a) + prim_to_float(b));
    } else if (is_unsigned_type(target_type)) {
        g_u32 res = prim_to_uint(a) + prim_to_uint(b);
        data = format_prim_data(res, target_type);
    } else {
        g_i32 res = prim_to_int(a) + prim_to_int(b);
        data = format_prim_data((g_u32)res, target_type);
    }

    *out_res = make_prim_val(data, STATE_OPEN, target_type);
    return 1;
}

static inline int type_safe_sub(prim_val a, prim_val b, prim_val *out_res) {
    prim_type target_type;
    if (!promote_types(get_prim_type(a), get_prim_type(b), &target_type)) return 0;

    word data;
    if (target_type == TYPE_FLOAT) {
        data = float_to_prim_data(prim_to_float(a) - prim_to_float(b));
    } else if (is_unsigned_type(target_type)) {
        g_u32 res = prim_to_uint(a) - prim_to_uint(b);
        data = format_prim_data(res, target_type);
    } else {
        g_i32 res = prim_to_int(a) - prim_to_int(b);
        data = format_prim_data((g_u32)res, target_type);
    }

    *out_res = make_prim_val(data, STATE_OPEN, target_type);
    return 1;
}

static inline int type_safe_mul(prim_val a, prim_val b, prim_val *out_res) {
    prim_type target_type;
    if (!promote_types(get_prim_type(a), get_prim_type(b), &target_type)) return 0;

    word data;
    if (target_type == TYPE_FLOAT) {
        data = float_to_prim_data(prim_to_float(a) * prim_to_float(b));
    } else if (is_unsigned_type(target_type)) {
        g_u32 res = prim_to_uint(a) * prim_to_uint(b);
        data = format_prim_data(res, target_type);
    } else {
        g_i32 res = prim_to_int(a) * prim_to_int(b);
        data = format_prim_data((g_u32)res, target_type);
    }

    *out_res = make_prim_val(data, STATE_OPEN, target_type);
    return 1;
}

static inline int type_safe_div(prim_val a, prim_val b, prim_val *out_res, int *div_by_zero) {
    if (div_by_zero) *div_by_zero = 0;
    prim_type target_type;
    if (!promote_types(get_prim_type(a), get_prim_type(b), &target_type)) return 0;

    word data;
    if (target_type == TYPE_FLOAT) {
        g_f32 fb = prim_to_float(b);
        if (fb == 0.0f) {
            if (div_by_zero) *div_by_zero = 1;
            return 0;
        }
        data = float_to_prim_data(prim_to_float(a) / fb);
    } else if (is_unsigned_type(target_type)) {
        g_u32 ub = prim_to_uint(b);
        if (ub == 0) {
            if (div_by_zero) *div_by_zero = 1;
            return 0;
        }
        g_u32 res = prim_to_uint(a) / ub;
        data = format_prim_data(res, target_type);
    } else {
        g_i32 ib = prim_to_int(b);
        if (ib == 0) {
            if (div_by_zero) *div_by_zero = 1;
            return 0;
        }
        g_i32 ia = prim_to_int(a);

        // Guard against INT32_MIN / -1 hardware CPU exception crash
        g_i32 res;
        if (ia == (g_i32)0x80000000 && ib == -1) {
            res = (g_i32)0x80000000; // Return INT32_MIN on overflow
        } else {
            res = ia / ib;
        }
        data = format_prim_data((g_u32)res, target_type);
    }

    *out_res = make_prim_val(data, STATE_OPEN, target_type);
    return 1;
}

static inline int type_safe_mod(prim_val a, prim_val b, prim_val *out_res, int *div_by_zero) {
    if (div_by_zero) *div_by_zero = 0;
    prim_type target_type;
    if (!promote_types(get_prim_type(a), get_prim_type(b), &target_type)) return 0;

    word data;
    if (target_type == TYPE_FLOAT) {
        g_f32 fb = prim_to_float(b);
        if (fb == 0.0f) {
            if (div_by_zero) *div_by_zero = 1;
            return 0;
        }
        data = float_to_prim_data(fmodf(prim_to_float(a), fb));
    } else if (is_unsigned_type(target_type)) {
        g_u32 ub = prim_to_uint(b);
        if (ub == 0) {
            if (div_by_zero) *div_by_zero = 1;
            return 0;
        }
        g_u32 res = prim_to_uint(a) % ub;
        data = format_prim_data(res, target_type);
    } else {
        g_i32 ib = prim_to_int(b);
        if (ib == 0) {
            if (div_by_zero) *div_by_zero = 1;
            return 0;
        }
        g_i32 ia = prim_to_int(a);

        g_i32 res;
        if (ia == (g_i32)0x80000000 && ib == -1) {
            res = 0;
        } else {
            res = ia % ib;
        }
        data = format_prim_data((g_u32)res, target_type);
    }

    *out_res = make_prim_val(data, STATE_OPEN, target_type);
    return 1;
}

#endif // TYPE_ARITHMETIC_H
