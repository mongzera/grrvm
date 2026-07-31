#ifndef GRRVM_TYPE_PROMOTION_H
#define GRRVM_TYPE_PROMOTION_H

#include <string.h>
#include <stdint.h>
#include "grrvm/vm.h"

// Explicit type rank for promotion
static inline int get_type_rank(prim_type type) {
    switch (type) {
        case TYPE_BYTE:  return 1;
        case TYPE_CHAR:  return 2;
        case TYPE_INT:   return 3;
        case TYPE_FLOAT: return 4;
        default:         return -1; // Non-numeric / pointer type
    }
}

// Determines the promoted target type between two primitive types
static inline int promote_types(prim_type a, prim_type b, prim_type *out_promoted) {
    int rank_a = get_type_rank(a);
    int rank_b = get_type_rank(b);

    if (rank_a < 0 || rank_b < 0) {
        return 0; // Promotion failed: incompatible non-numeric type
    }

    if (out_promoted != NULL) {
        *out_promoted = (rank_a >= rank_b) ? a : b;
    }
    return 1;
}

// Helper to extract a C double representation from a raw primitive word payload
static inline double prim_to_double(prim_val val) {
    prim_type type = (prim_type)(val.metadata & 0x0F);

    switch (type) {
        case TYPE_BYTE:  return (double)(uint8_t)val.data;
        case TYPE_CHAR:  return (double)(char)val.data;
        case TYPE_INT:   return (double)(int32_t)val.data;
        case TYPE_FLOAT: {
            float f;
            uint32_t raw = (uint32_t)val.data;
            memcpy(&f, &raw, sizeof(float));
            return (double)f;
        }
        default: return 0.0;
    }
}

// Converts a raw standard double back to a prim_val word payload
static inline word double_to_prim_data(double val, prim_type target_type) {
    switch (target_type) {
        case TYPE_BYTE:  return (word)(uint8_t)val;
        case TYPE_CHAR:  return (word)(char)val;
        case TYPE_INT:   return (word)(int32_t)val;
        case TYPE_FLOAT: {
            float f = (float)val;
            uint32_t raw;
            memcpy(&raw, &f, sizeof(float));
            return (word)raw;
        }
        default: return 0;
    }
}

// --- TYPE-SAFE ARITHMETIC PRIMITIVES ---

static inline int type_safe_add(prim_val a, prim_val b, prim_val *out_res) {
    prim_type type_a = (prim_type)(a.metadata & 0x0F);
    prim_type type_b = (prim_type)(b.metadata & 0x0F);
    prim_type target_type;

    if (!promote_types(type_a, type_b, &target_type)) return 0;

    double res = prim_to_double(a) + prim_to_double(b);
    *out_res = make_prim_val(double_to_prim_data(res, target_type), STATE_OPEN, target_type);
    return 1;
}

static inline int type_safe_sub(prim_val a, prim_val b, prim_val *out_res) {
    prim_type type_a = (prim_type)(a.metadata & 0x0F);
    prim_type type_b = (prim_type)(b.metadata & 0x0F);
    prim_type target_type;

    if (!promote_types(type_a, type_b, &target_type)) return 0;

    double res = prim_to_double(a) - prim_to_double(b);
    *out_res = make_prim_val(double_to_prim_data(res, target_type), STATE_OPEN, target_type);
    return 1;
}

static inline int type_safe_mul(prim_val a, prim_val b, prim_val *out_res) {
    prim_type type_a = (prim_type)(a.metadata & 0x0F);
    prim_type type_b = (prim_type)(b.metadata & 0x0F);
    prim_type target_type;

    if (!promote_types(type_a, type_b, &target_type)) return 0;

    double res = prim_to_double(a) * prim_to_double(b);
    *out_res = make_prim_val(double_to_prim_data(res, target_type), STATE_OPEN, target_type);
    return 1;
}

static inline int type_safe_div(prim_val a, prim_val b, prim_val *out_res, int *div_by_zero) {
    *div_by_zero = 0;
    prim_type type_a = (prim_type)(a.metadata & 0x0F);
    prim_type type_b = (prim_type)(b.metadata & 0x0F);
    prim_type target_type;

    if (!promote_types(type_a, type_b, &target_type)) return 0;

    double val_b = prim_to_double(b);
    if (val_b == 0.0) {
        *div_by_zero = 1;
        return 0;
    }

    double res = prim_to_double(a) / val_b;
    *out_res = make_prim_val(double_to_prim_data(res, target_type), STATE_OPEN, target_type);
    return 1;
}

static inline int type_safe_mod(prim_val a, prim_val b, prim_val *out_res, int *div_by_zero) {
    *div_by_zero = 0;
    prim_type type_a = (prim_type)(a.metadata & 0x0F);
    prim_type type_b = (prim_type)(b.metadata & 0x0F);
    prim_type target_type;

    if (!promote_types(type_a, type_b, &target_type)) return 0;

    // Modulo is invalid on floating-point types in strict integer semantics
    if (target_type == TYPE_FLOAT) return 0;

    int32_t val_b = (int32_t)prim_to_double(b);
    if (val_b == 0) {
        *div_by_zero = 1;
        return 0;
    }

    int32_t res = (int32_t)prim_to_double(a) % val_b;
    *out_res = make_prim_val(double_to_prim_data((double)res, target_type), STATE_OPEN, target_type);
    return 1;
}

#endif // GRRVM_TYPE_PROMOTION_H
