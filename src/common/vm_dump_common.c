
#include "grrvm/common/vm_dump_common.h"
#include <stdint.h>

static const char* get_state_str(prim_state state) {
    switch (state) {
        case STATE_OPEN:     return "OPEN";
        case STATE_LOCKED:   return "LOCKED";
        case STATE_CONSTANT: return "CONSTANT";
        case STATE_ERROR:
        default:             return "ERROR/UNKNOWN";
    }
}

static const char* get_type_str(prim_type type) {
    switch (type) {
        case TYPE_NULL:      return "NULL";
        case TYPE_U8:        return "U8";
        case TYPE_U16:       return "U16";
        case TYPE_U32:       return "U32";
        case TYPE_U64:       return "U64 (UNSUPPORTED)";
        case TYPE_I8:        return "I8";
        case TYPE_I16:       return "I16";
        case TYPE_I32:       return "I32";
        case TYPE_I64:       return "I64 (UNSUPPORTED)";
        case TYPE_FLOAT:     return "FLOAT";
        case TYPE_LENGTH:    return "LENGTH";
        case TYPE_REFERENCE: return "REFERENCE";
        default:             return "INVALID_TYPE";
    }
}

void dump_heap_to_stream(FILE *out, const prim_val *heap, size_t capacity) {
    if (!heap) {
        fprintf(out, "Heap pointer is NULL.\n");
        return;
    }

    fprintf(out, "=========================================================================================================\n");
    fprintf(out, "| %-5s | %-12s | %-18s | %-8s | %-16s | %-12s |\n",
            "INDEX", "STATE", "TYPE", "GC MARK", "VALUE (DEC)", "VALUE (HEX)");
    fprintf(out, "=========================================================================================================\n");

    for (size_t i = 0; i < capacity; i++) {
        prim_state state = get_prim_state(heap[i]);
        prim_type type   = get_prim_type(heap[i]);

        const char *state_str = get_state_str(state);
        const char *type_str  = get_type_str(type);

        fprintf(out, "| %-5zu | %-12s | %-18s | 0x%02X     | ",
                i, state_str, type_str, heap[i].gc_mark);

        switch (type) {
            case TYPE_NULL:
                fprintf(out, "%-16s | ", "NULL");
                break;
            case TYPE_FLOAT:
                fprintf(out, "%-16.4f | ", heap[i].float_data);
                break;
            case TYPE_I8:
                fprintf(out, "%-16d | ", (int8_t)(heap[i].data & 0xFF));
                break;
            case TYPE_I16:
                fprintf(out, "%-16d | ", (int16_t)(heap[i].data & 0xFFFF));
                break;
            case TYPE_I32:
                fprintf(out, "%-16d | ", (int32_t)heap[i].data);
                break;
            case TYPE_U8:
                fprintf(out, "%-16u | ", (uint8_t)(heap[i].data & 0xFF));
                break;
            case TYPE_U16:
                fprintf(out, "%-16u | ", (uint16_t)(heap[i].data & 0xFFFF));
                break;
            case TYPE_U32:
            case TYPE_LENGTH:
            case TYPE_REFERENCE:
            default:
                fprintf(out, "%-16u | ", heap[i].data);
                break;
        }

        fprintf(out, "0x%08X   |\n", heap[i].data);
    }
    fprintf(out, "=========================================================================================================\n");
}
