#ifndef GRRVM_PARSER_BYTECODE_PARSE_H
#define GRRVM_PARSER_BYTECODE_PARSE_H

#include "grrvm/prim_val.h"
#include "grrvm/vm.h"
#include "grrvm/vm_alloc.h"
#include "grrvm/vm_global_table.h"
#include "grrvm/vm_log.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>

static inline prim_type asm_to_vm_primtype(byte asm_type){
    switch (asm_type) {
        case 0x01: return TYPE_U8;
        case 0x02: return TYPE_U16;
        case 0x03: return TYPE_U32;

        case 0x05: return TYPE_I8;
        case 0x06: return TYPE_I16;
        case 0x07: return TYPE_I32;

        case 0x09: return TYPE_FLOAT;

        default:   return TYPE_NULL;
    }
}

static inline byte asm_to_vm_metadata(uint32_t asm_metadata){
    prim_state is_constant = ((byte)(asm_metadata >> 24)) ? STATE_CONSTANT : STATE_OPEN;
    byte type = (byte)(asm_metadata & 0xFF);
    prim_type t = asm_to_vm_primtype(type);

    return pack_meta(is_constant, t);
}

typedef struct global_data {
    uint32_t header;
    uint32_t length;
    uint32_t* data;
    struct global_data *next;
    struct global_data *prev;
} global_data;

static inline global_data* create_global_row(global_data* current) {
    global_data* new_row = malloc(sizeof(global_data));
    if (new_row == NULL) {
        vm_error("GLOBAL DATA PARSE", "Cannot create new Global Data Row!");
        return NULL;
    }

    new_row->header = 0;
    new_row->length = 0;
    new_row->data = NULL;
    new_row->next = NULL;
    new_row->prev = current;

    if (current != NULL) {
        current->next = new_row;
    }

    return new_row;
}

static inline void free_global_row(global_data* last_row) {
    global_data* this_row = last_row;

    // Loop until we hit NULL, which safely frees every node including the head
    while (this_row != NULL) {
        global_data* prev_row = this_row->prev;

        // Free internal data buffer first if it was dynamically allocated
        free(this_row->data);

        // Free the row itself
        free(this_row);

        this_row = prev_row;
    }
}

static inline global_data* find_genesis_row(global_data *nth_row){
    global_data *current = nth_row;
    assert(current != NULL);

    while (current->prev != NULL) {
        current = current->prev;
    }

    return current;
}

static inline void allocate_global_heap_block(VM* vm, global_data* last_row, uint32_t total_heap_block_size){
    global_data *current_row = find_genesis_row(last_row);
    vm->global_heap_block_address = g_malloc_heap_block(vm, total_heap_block_size, 1);

    uint32_t current_mem_slot = vm->global_heap_block_address + 1; // skip the global_heap_block_address since this will describe the heap block size for the GC to handle cleanly
    while (current_row != NULL) {
        byte prim_metadata = asm_to_vm_metadata(current_row->header);
        word length = current_row->length;


        vm->ram[current_mem_slot++] = (prim_val){
            .metadata = pack_meta(STATE_LOCKED, TYPE_LENGTH),
            .data = length
        };



        for(uint32_t i = 0; i < length; i++){
            vm->ram[current_mem_slot++] = (prim_val){
                .metadata = prim_metadata,
                .data = current_row->data[i]
            };
        }

        current_row = current_row->next;
    }

    free_global_row(last_row);
}


static inline uint32_t get_buffer_next(const uint32_t *buffer, size_t* current_offset){
    return buffer[(*current_offset)++];
}



static inline int bytecode_parser(const uint32_t *buffer, size_t buf_word_count, VM* vm) {
    if (buffer == NULL || vm == NULL) return -1;

    // Check if the buffer contains at least the 3 header words
    if (buf_word_count < 3) {
        vm_error("GRRVM BYTECODE PARSER", "Invalid bytecode! ERR: 0x1");
        return -1;
    }

    size_t offset = 0;


    // Direct array access on uint32_t pointer retrieves the 32-bit values directly
    uint32_t program_offset = get_buffer_next(buffer, &offset);
    vm->program_size  = get_buffer_next(buffer, &offset);
    vm->_program_start  = get_buffer_next(buffer, &offset);

    if(program_offset + vm->program_size > buf_word_count){
        vm_error("GRRVM BYTECODE PARSER", "Invalid bytecode! ERR: 0x2");
        return -1;
    }


    //load program
    for(word i = 0; i < vm->program_size; i++){
        vm->program[i] = buffer[program_offset+i];
        vm_info("PROGRAM LOAD", "INSTRUCTION: %d", vm->program[i]);
    }

    //load memory
    global_data* current_row = NULL;
    uint32_t row_count = 0;
    uint32_t total_data = 0;
    uint32_t total_heap_block_size = 0;

    vm_info("Bytecode Loader", "Offset: %u, Program Offset: %u", (unsigned)offset, (unsigned)program_offset);

    while(offset < program_offset){
        current_row = create_global_row(current_row);
        row_count++;

        if(current_row == NULL){
            exit(-1);
        }

        uint32_t asm_metadata = get_buffer_next(buffer, &offset);
        uint32_t length = get_buffer_next(buffer, &offset);
        byte prim_metadata = asm_to_vm_metadata(asm_metadata);

        // for arrays
        current_row->header = prim_metadata;
        current_row->length = length;
        current_row->data = malloc(sizeof(uint32_t) * length);
        total_data += length;

        for(uint32_t i = 0; i < length; i++){
            current_row->data[i] = get_buffer_next(buffer, &offset);
        }
    }

    total_heap_block_size = row_count + total_data;
    printf("REQ HEAP BLOCK SIZE: %u\n", total_heap_block_size);
    allocate_global_heap_block(vm, current_row, total_heap_block_size);

    return 0;
}

#endif /* GRRVM_PARSER_BYTECODE_PARSE_H */
