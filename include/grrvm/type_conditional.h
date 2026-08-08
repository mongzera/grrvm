#ifndef TYPE_CONDITIONAL_H
#define TYPE_CONDITIONAL_H

#include "grrvm/type_promotion.h"
#include "grrvm/types.h"
#include "grrvm/vm.h"
#include "grrvm/vm_math.h"

static inline int type_safe_eq(prim_val a, prim_val b, prim_val *out_res) {
    prim_type target_type;
    if (!promote_types(get_prim_type(a), get_prim_type(b), &target_type)) return 0;

    int cond = (target_type == TYPE_FLOAT)
        ? (VM_FABS(prim_to_float(a) - prim_to_float(b)) < EPSILON)
        : (a.data == b.data);

    *out_res = make_prim_val((word)cond, STATE_OPEN, TYPE_INT);
    return 1;
}

static inline int type_safe_neq(prim_val a, prim_val b, prim_val *out_res) {
    return !type_safe_eq(a, b, out_res);
}

static inline int type_safe_lt(prim_val a, prim_val b, prim_val *out_res) {
    prim_type target_type;
    if (!promote_types(get_prim_type(a), get_prim_type(b), &target_type)) return 0;

    int cond = (target_type == TYPE_FLOAT) ? (prim_to_float(a) < prim_to_float(b)) : ((int)a.data < (int)b.data);

    *out_res = make_prim_val((word)cond, STATE_OPEN, TYPE_INT);
    return 1;
}

static inline int type_safe_lte(prim_val a, prim_val b, prim_val *out_res) {
    prim_type target_type;
    if (!promote_types(get_prim_type(a), get_prim_type(b), &target_type)) return 0;

    int cond = (target_type == TYPE_FLOAT)
        ? (VM_FABS(prim_to_float(a) - prim_to_float(b)) >= EPSILON)
        : (a.data != b.data);

    *out_res = make_prim_val((word)cond, STATE_OPEN, TYPE_INT);
    return 1;
}
#endif
