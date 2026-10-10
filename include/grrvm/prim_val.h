#ifndef GRRVM_PRIM_VAL_H
#define GRRVM_PRIM_VAL_H

/*
 * The VM's value model: prim_val and its state/type metadata.
 *
 * Leaf header - depends only on types.h and vm_log.h, and knows nothing
 * about VM / VM_Thread. Everything that only needs to *handle values*
 * (type_*.h, vm_alloc.h) includes this instead of vm.h.
 */

#include <stdlib.h>
#include "grrvm/types.h"
#include "grrvm/vm_log.h"

typedef enum prim_state {
    STATE_ERROR = -1,
    STATE_OPEN = 0x0,
    STATE_LOCKED,
    STATE_CONSTANT,
} prim_state;

typedef enum prim_type {
    TYPE_NULL = 0x0,
    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_U64, // NOTE: Unsupported
    TYPE_I8,
    TYPE_I16,
    TYPE_I32,
    TYPE_I64, // NOTE: Unsupported
    TYPE_FLOAT,
    TYPE_LENGTH,
    TYPE_REFERENCE,
    PRIMTYPE_COUNT
} prim_type;

typedef struct prim_val {
    union {
        word data;
        float float_data;
    };

    byte metadata; // metadata 0xEF, E - state, F - type
    byte gc_mark;
} prim_val;

static inline byte pack_meta(prim_state state, prim_type type) {
    return (byte)((((byte)state & 0x0F) << 4) | ((byte)type & 0x0F));
}

static inline void set_val_meta(prim_val *pv, prim_state state, prim_type type) {
    // do prim_type checks
    if ((int)type < 0 || type >= PRIMTYPE_COUNT) {
        vm_error("TYPE ERROR", "Invalid datatype! Code: %d", (int)type);
        exit(-1);
    }
    pv->metadata = pack_meta(state, type);
}

static inline void set_val_state(prim_val *pv, prim_state state) {
    pv->metadata = (pv->metadata & 0x0F) | (byte)(((byte)state & 0x0F) << 4);
}

static inline void set_val_type(prim_val *pv, prim_type type) {
    pv->metadata = (pv->metadata & 0xF0) | (byte)((byte)type & 0x0F);
}

static inline prim_val make_prim_val(word data, prim_state state, prim_type type) {
    prim_val pv;
    pv.data = data;
    pv.gc_mark = 0; // THIS IS IMPORTANT, SET TO 0 When making a new value
    pv.metadata = pack_meta(state, type);
    return pv;
}

static inline prim_type get_prim_type(prim_val val) {
    return ((prim_type)((val).metadata & 0x0F));
}

static inline prim_state get_prim_state(prim_val val) {
    return ((prim_state)(((val).metadata >> 4) & 0x0F));
}

#endif /* GRRVM_PRIM_VAL_H */
