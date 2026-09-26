#ifndef TYPE_CHECKS_H
#define TYPE_CHECKS_H

#include "grrvm/type_conversion.h"
#include "grrvm/vm.h"

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

static inline int type_check_float(prim_val p) {
    return (get_prim_type(p) == TYPE_FLOAT);
}


#endif
