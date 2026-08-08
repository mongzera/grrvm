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

#endif
