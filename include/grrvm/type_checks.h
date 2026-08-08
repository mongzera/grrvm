#ifndef TYPE_CHECKS_H
#define TYPE_CHECKS_H

#include "grrvm/vm.h"

static inline int is_unsigned_type(prim_type t) {
    return (t == TYPE_UINT || t == TYPE_BYTE || t == TYPE_REFERENCE);
}

// Extract data as a signed integer (g_int) with proper sign-extension
static inline g_int extract_signed(prim_val p) {
    if (get_prim_type(p) == TYPE_BYTE) return (g_int)(int8_t)(p.data & 0xFF);

    return (g_int)p.data;
}

#endif
