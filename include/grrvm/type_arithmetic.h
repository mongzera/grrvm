#ifndef TYPE_ARITHMETIC_H
#define TYPE_ARITHMETIC_H

// --- TYPE-SAFE ARITHMETIC PRIMITIVES ---

#include "grrvm/type_promotion.h"
#include "grrvm/vm.h"
static inline int type_safe_add(prim_val a, prim_val b, prim_val *out_res) {
    prim_type target_type;
    if (!promote_types(get_prim_type(a), get_prim_type(b), &target_type)) return 0;

    word data;
    if (target_type == TYPE_FLOAT) {
        data = float_to_prim_data(prim_to_float(a) + prim_to_float(b), target_type);
    } else {
        data = a.data + b.data;
    }

    *out_res = make_prim_val(data, STATE_OPEN, target_type);
    return 1;
}

static inline int type_safe_sub(prim_val a, prim_val b, prim_val *out_res) {
    prim_type target_type;
    if (!promote_types(get_prim_type(a), get_prim_type(b), &target_type)) return 0;

    word data;
    if (target_type == TYPE_FLOAT) {
        data = float_to_prim_data(prim_to_float(a) - prim_to_float(b), target_type);
    } else {
        data = a.data - b.data;
    }

    *out_res = make_prim_val(data, STATE_OPEN, target_type);
    return 1;
}

static inline int type_safe_mul(prim_val a, prim_val b, prim_val *out_res) {
    prim_type target_type;
    if (!promote_types(get_prim_type(a), get_prim_type(b), &target_type)) return 0;

    word data;
    if (target_type == TYPE_FLOAT) {
        data = float_to_prim_data(prim_to_float(a) * prim_to_float(b), target_type);
    } else {
        data = a.data * b.data;
    }

    *out_res = make_prim_val(data, STATE_OPEN, target_type);
    return 1;
}

static inline int type_safe_div(prim_val a, prim_val b, prim_val *out_res, int *div_by_zero) {
    *div_by_zero = 0;
    prim_type target_type;
    if (!promote_types(get_prim_type(a), get_prim_type(b), &target_type)) return 0;

    if (target_type == TYPE_FLOAT) {
        float fb = prim_to_float(b);
        if (fb == 0.0f) { *div_by_zero = 1; return 0; }
        *out_res = make_prim_val(float_to_prim_data(prim_to_float(a) / fb, target_type), STATE_OPEN, TYPE_FLOAT);
    } else {
        if (b.data == 0) { *div_by_zero = 1; return 0; }

        word res = (target_type == TYPE_INT)
            ? (word)((int32_t)a.data / (int32_t)b.data)
            : (a.data / b.data);

        *out_res = make_prim_val(res, STATE_OPEN, target_type);
    }
    return 1;
}

static inline int type_safe_mod(prim_val a, prim_val b, prim_val *out_res, int *div_by_zero) {
    *div_by_zero = 0;
    prim_type target_type;
    if (!promote_types(get_prim_type(a), get_prim_type(b), &target_type)) return 0;

    if (target_type == TYPE_FLOAT || b.data == 0) {
        if (b.data == 0) *div_by_zero = 1;
        return 0;
    }

    word res = (target_type == TYPE_INT)
        ? (word)((int32_t)a.data % (int32_t)b.data)
        : (a.data % b.data);

    *out_res = make_prim_val(res, STATE_OPEN, target_type);
    return 1;
}

#endif
