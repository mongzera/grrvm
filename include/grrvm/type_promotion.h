#ifndef GRRVM_TYPE_PROMOTION_H
#define GRRVM_TYPE_PROMOTION_H

#include <stdint.h>
#include <string.h>
#include "grrvm/type_checks.h"
#include "grrvm/type_conversion.h"
#include "grrvm/types.h"
#include "grrvm/vm.h"

/* --- TYPE RANKING & PROMOTION --- */
static inline int get_type_rank(prim_type type) {
    switch (type) {
        case TYPE_I8:           return 1;
        case TYPE_U8:           return 2;
        case TYPE_I16:          return 3;
        case TYPE_U16:          return 4;
        case TYPE_I32:          return 5;
        case TYPE_U32:          return 6;
        case TYPE_FLOAT:        return 7;
        case TYPE_REFERENCE:    // Non-numeric / Object reference
        default:                return -1;
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
    // If the signed number is negative, it is always less than an unsigned number
    if (a < 0) return -1;

    // Otherwise, they are both positive, so we can safely cast and compare as unsigned
    g_uint ua = (g_uint) a;
    return (ua > b) - (ua < b);
}

static inline int compare_prim(prim_val a, prim_val b) {
    prim_type ta = get_prim_type(a);
    prim_type tb = get_prim_type(b);

    // 1. Float Path (Explicit float involved)
    if (ta == TYPE_FLOAT || tb == TYPE_FLOAT) {
        g_f32 fa = prim_to_float(a);
        g_f32 fb = prim_to_float(b);
        return (fa > fb) - (fa < fb);
    }

    // 2. Both Unsigned
    if (is_unsigned_type(ta) && is_unsigned_type(tb)) {
        g_uint ua = prim_to_uint(a), ub = prim_to_uint(b);
        return (ua > ub) - (ua < ub);
    }

    // 3. Both Signed (FIXED: Uses prim_to_int to ensure proper sign extension)
    if (!is_unsigned_type(ta) && !is_unsigned_type(tb)) {
        g_int ia = prim_to_int(a), ib = prim_to_int(b);
        return (ia > ib) - (ia < ib);
    }

    // 4. Mixed Signed/Unsigned (FIXED: Uses prim_to_int / prim_to_uint)
    if (is_unsigned_type(ta)) {
        // 'a' is unsigned, 'b' is signed. We invert the result since the helper expects (signed, unsigned)
        return signed_unsigned_comparison(prim_to_int(b), prim_to_uint(a)) * -1;
    }

    // 'a' is signed, 'b' is unsigned
    return signed_unsigned_comparison(prim_to_int(a), prim_to_uint(b));
}

// Returns 1 if a <= b, 0 otherwise
static inline int compare_prim_lte(prim_val a, prim_val b) {
    return compare_prim(a, b) <= 0;
}

// Returns 1 if a >= b, 0 otherwise
static inline int compare_prim_gte(prim_val a, prim_val b) {
    return compare_prim(a, b) >= 0;
}

#endif // GRRVM_TYPE_PROMOTION_H
