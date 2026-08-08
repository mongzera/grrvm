#ifndef VM_H
#define VM_H

#include "config.h"
#include "types.h"


typedef enum prim_state {
    STATE_OPEN = 0x0,
    STATE_LOCKED,
    STATE_CONSTANT,
    STATE_GARBAGE
} prim_state;

typedef enum prim_type {
    TYPE_U8 = 0x0,
    TYPE_I8,
    TYPE_U16,
    TYPE_I16,
    TYPE_U32,
    TYPE_I32,
    TYPE_FLOAT,
    TYPE_REFERENCE
} prim_type;

typedef struct prim_val{
    word data;
    byte metadata; // metadata 0xEF, E - state, F - type
} prim_val;


typedef struct VM_CallStack{
    word previous_pc;
    word previous_sfp;
} VM_CallStack;


typedef struct VM_Thread{
    byte status;
    word pc;
    word pc_checkpoint;
    g_int sp;
    g_int csp;
    g_int sfp;
    struct VM* vm;
    prim_val op_stack[VM_OP_STACK_MAX];
    VM_CallStack call_stack[VM_CALL_STACK_MAX];

} VM_Thread;

typedef struct VM {
    word _program_start;
    word program_size;
    word program[VM_PROGRAM_MAX_SIZE];
    prim_val ram[VM_MAX_RAM];
    VM_Thread vm_threads[VM_MAX_THREADS];
} VM;

void vm_loop(VM* vm);
void vm_terminate(VM* vm);
void vm_new_thread(VM* vm, word program_counter, int id);

static inline byte pack_meta(prim_state state, prim_type type) {
    return (byte)((((byte)state & 0x0F) << 4) | ((byte)type & 0x0F));
}

static inline void set_val_meta(prim_val *pv, prim_state state, prim_type type) {
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
    pv.metadata = pack_meta(state, type);
    return pv;
}

static inline prim_type get_prim_type(prim_val val){
    return ((prim_type)((val).metadata & 0x0F));
}

static inline prim_state get_prim_state(prim_val val){
    return ((prim_state)(((val).metadata >> 4) & 0x0F));
}

static inline void set_thread_active(VM_Thread* thread) {
    thread->status |= (byte)0x01;
}

static inline void set_thread_inactive(VM_Thread* thread) {
    thread->status &= (byte)(~0x01);
}

static inline byte is_thread_active(const VM_Thread* thread) {
    return (thread->status & 0x01) != 0;
}

#endif
