#ifndef GRRVM_TYPE_PROMOTION_H
#define GRRVM_TYPE_PROMOTION_H

#include <stdint.h>
#include <string.h>
#include "grrvm/vm.h"
/* --- TYPE RANKING & PROMOTION --- */
static inline int get_type_rank(prim_type type) {
    switch (type) {
        case TYPE_BYTE:      return 1;
        case TYPE_CHAR:      return 2;
        case TYPE_UINT:      return 3;
        case TYPE_INT:       return 4;
        case TYPE_FLOAT:     return 5;
        case TYPE_REFERENCE: return -1; // Non-numeric / Object reference
        default:             return -1;
    }
}

static inline int promote_types(prim_type a, prim_type b, prim_type *out_promoted) {
    int rank_a = get_type_rank(a);
    int rank_b = get_type_rank(b);

    if (rank_a < 0 || rank_b < 0) return 0; // Invalid / Incompatible types

    if (out_promoted != NULL) {
        *out_promoted = (rank_a >= rank_b) ? a : b;
    }
    return 1;
}

static inline float prim_to_float(prim_val val) {
    switch (get_prim_type(val)) {
        case TYPE_BYTE:  return (float)(uint8_t)val.data;
        case TYPE_CHAR:  return (float)(int8_t)val.data;
        case TYPE_UINT:  return (float)(uint32_t)val.data;
        case TYPE_INT:   return (float)(int32_t)val.data;
        case TYPE_FLOAT: {
            float f;
            uint32_t raw = (uint32_t)val.data;
            memcpy(&f, &raw, sizeof(float));
            return f;
        }
        default: return 0.0f;
    }
}

static inline word float_to_prim_data(float f, prim_type target_type) {
    switch (target_type) {
        case TYPE_BYTE:  return (word)(uint8_t)f;
        case TYPE_CHAR:  return (word)(int8_t)f;
        case TYPE_UINT:  return (word)(uint32_t)f;
        case TYPE_INT:   return (word)(int32_t)f;
        case TYPE_FLOAT: {
            uint32_t raw;
            memcpy(&raw, &f, sizeof(float));
            return (word)raw;
        }
        default: return 0;
    }
}

#endif // GRRVM_TYPE_PROMOTION_H
