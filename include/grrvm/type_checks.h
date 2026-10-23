#ifndef GRRVM_TYPE_CHECKS_H
#define GRRVM_TYPE_CHECKS_H

#include "grrvm/types.h"
#include "grrvm/prim_val.h"
#include "grrvm/type_conversion.h"

static inline int is_unsigned_type(prim_type t) {
    return (t == TYPE_U8 || t == TYPE_U16 || t == TYPE_U32 || t == TYPE_REFERENCE);
}

// Extract data as a signed integer (g_int) with proper sign-extension
static inline g_int extract_signed(prim_val p) {
    return prim_to_int(p);
}

static inline int type_check_int(prim_val p) {
    switch (get_prim_type(p)) {
        case TYPE_I8:
        case TYPE_I16:
        case TYPE_I32:
        case TYPE_U8:
        case TYPE_U16:
        case TYPE_U32:
            return 1;
        default:
            return 0;
    }
}

static inline int type_check_non_negative(prim_val p) {
    return (extract_signed(p) >= 0);
}

static inline int type_check_float(prim_val p) {
    return (get_prim_type(p) == TYPE_FLOAT);
}


#endif /* GRRVM_TYPE_CHECKS_H */
