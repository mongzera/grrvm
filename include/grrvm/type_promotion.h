#ifndef GRRVM_TYPE_PROMOTION_H
#define GRRVM_TYPE_PROMOTION_H

#include <stdint.h>
#include <string.h>
#include "grrvm/type_checks.h"
#include "grrvm/types.h"
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

static inline int signed_unsigned_comparison(g_int a, g_uint b){
    if(a < 0) return -1;
    g_uint ua = (g_uint) a;
    return (ua > b) - (ua < b);
}

static inline int compare_prim(prim_val a, prim_val b) {
    prim_type ta = get_prim_type(a);
    prim_type tb = get_prim_type(b);

    // 1. Float Path (Explicit float involved)
    if (ta == TYPE_FLOAT || tb == TYPE_FLOAT) {
        g_float fa = prim_to_float(a);
        g_float fb = prim_to_float(b);
        return (fa > fb) - (fa < fb);
    }

    if(is_unsigned_type(ta) && is_unsigned_type(tb)){
        g_uint ua = (g_uint)a.data, ub = (g_uint)b.data;

        return (ua > ub) - (ua < ub);
    }

    if(!is_unsigned_type(ta) && !is_unsigned_type(tb)){
        g_int ua = (g_int)a.data, ub = (g_int)b.data;

        return (ua > ub) - (ua < ub);
    }

    if(is_unsigned_type(ta)){
        return signed_unsigned_comparison((g_int) b.data, (g_uint) a.data) * -1;
    }

    return signed_unsigned_comparison((g_int) a.data, (g_uint) b.data);
}

static inline int compare_prim_lte(prim_val a, prim_val b){

}

static inline int compare_prim_gte(prim_val a, prim_val b){

}

static inline word float_to_prim_data(float f, prim_type target_type) {
    switch (target_type) {
        case TYPE_BYTE:  return (word)(g_byte)f;
        case TYPE_CHAR:  return (word)(g_char)f;
        case TYPE_UINT:  return (word)(g_uint)f;
        case TYPE_INT:   return (word)(g_int)f;
        case TYPE_FLOAT: {
            uint32_t raw;
            memcpy(&raw, &f, sizeof(g_float));
            return (word)raw;
        }
        default: return 0;
    }
}

#endif // GRRVM_TYPE_PROMOTION_H
