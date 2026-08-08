#ifndef TYPE_CONDITIONAL_H
#define TYPE_CONDITIONAL_H

// --- TYPE-SAFE CONDITIONAL & RELATIONAL PRIMITIVES ---

#include "grrvm/type_checks.h"
#include "grrvm/type_conversion.h"
#include "grrvm/type_promotion.h"
#include "grrvm/types.h"
#include "grrvm/vm.h"
#include "grrvm/vm_math.h"

static inline int type_safe_eq(prim_val a, prim_val b, prim_val *out_res) {
    prim_type target_type;
    if (!promote_types(get_prim_type(a), get_prim_type(b), &target_type)) return 0;

    int cond;
    if (target_type == TYPE_FLOAT) {
        cond = (VM_FABS(prim_to_float(a) - prim_to_float(b)) < EPSILON);
    } else if (is_unsigned_type(target_type)) {
        cond = (prim_to_uint(a) == prim_to_uint(b));
    } else {
        cond = (prim_to_int(a) == prim_to_int(b));
    }

    *out_res = make_prim_val((word)cond, STATE_OPEN, TYPE_I32);
    return 1;
}

static inline int type_safe_neq(prim_val a, prim_val b, prim_val *out_res) {
    prim_type target_type;
    if (!promote_types(get_prim_type(a), get_prim_type(b), &target_type)) return 0;

    int cond;
    if (target_type == TYPE_FLOAT) {
        cond = (VM_FABS(prim_to_float(a) - prim_to_float(b)) >= EPSILON);
    } else if (is_unsigned_type(target_type)) {
        cond = (prim_to_uint(a) != prim_to_uint(b));
    } else {
        cond = (prim_to_int(a) != prim_to_int(b));
    }

    *out_res = make_prim_val((word)cond, STATE_OPEN, TYPE_I32);
    return 1;
}

static inline int type_safe_lt(prim_val a, prim_val b, prim_val *out_res) {
    prim_type target_type;
    if (!promote_types(get_prim_type(a), get_prim_type(b), &target_type)) return 0;

    int cond;
    if (target_type == TYPE_FLOAT) {
        cond = (prim_to_float(a) < prim_to_float(b));
    } else if (is_unsigned_type(target_type)) {
        cond = (prim_to_uint(a) < prim_to_uint(b));
    } else {
        cond = (prim_to_int(a) < prim_to_int(b));
    }

    *out_res = make_prim_val((word)cond, STATE_OPEN, TYPE_I32);
    return 1;
}

static inline int type_safe_lte(prim_val a, prim_val b, prim_val *out_res) {
    prim_type target_type;
    if (!promote_types(get_prim_type(a), get_prim_type(b), &target_type)) return 0;

    int cond;
    if (target_type == TYPE_FLOAT) {
        g_f32 fa = prim_to_float(a);
        g_f32 fb = prim_to_float(b);
        cond = (fa <= fb) || (VM_FABS(fa - fb) < EPSILON);
    } else if (is_unsigned_type(target_type)) {
        cond = (prim_to_uint(a) <= prim_to_uint(b));
    } else {
        cond = (prim_to_int(a) <= prim_to_int(b));
    }

    *out_res = make_prim_val((word)cond, STATE_OPEN, TYPE_I32);
    return 1;
}

static inline int type_safe_gt(prim_val a, prim_val b, prim_val *out_res) {
    prim_type target_type;
    if (!promote_types(get_prim_type(a), get_prim_type(b), &target_type)) return 0;

    int cond;
    if (target_type == TYPE_FLOAT) {
        cond = (prim_to_float(a) > prim_to_float(b));
    } else if (is_unsigned_type(target_type)) {
        cond = (prim_to_uint(a) > prim_to_uint(b));
    } else {
        cond = (prim_to_int(a) > prim_to_int(b));
    }

    *out_res = make_prim_val((word)cond, STATE_OPEN, TYPE_I32);
    return 1;
}

static inline int type_safe_gte(prim_val a, prim_val b, prim_val *out_res) {
    prim_type target_type;
    if (!promote_types(get_prim_type(a), get_prim_type(b), &target_type)) return 0;

    int cond;
    if (target_type == TYPE_FLOAT) {
        g_f32 fa = prim_to_float(a);
        g_f32 fb = prim_to_float(b);
        cond = (fa >= fb) || (VM_FABS(fa - fb) < EPSILON);
    } else if (is_unsigned_type(target_type)) {
        cond = (prim_to_uint(a) >= prim_to_uint(b));
    } else {
        cond = (prim_to_int(a) >= prim_to_int(b));
    }

    *out_res = make_prim_val((word)cond, STATE_OPEN, TYPE_I32);
    return 1;
}

#endif // TYPE_CONDITIONAL_H
