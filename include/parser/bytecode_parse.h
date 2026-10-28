#ifndef GRRVM_PARSER_BYTECODE_PARSE_H
#define GRRVM_PARSER_BYTECODE_PARSE_H

#include "grrvm/prim_val.h"
#include "grrvm/vm.h"
#include "grrvm/vm_alloc.h"
#include "grrvm/vm_dump.h"
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

/* The assembler encodes a global-data record header as:
 *   bit  7    : constant flag (0x80)
 *   bits 6..0 : primitive type id
 * Everything above bit 7 must be zero, and the type id must be one the
 * VM actually supports. Used by the loader to reject malformed images
 * before any runtime state is built from them. */
static inline int is_supported_asm_header(uint32_t asm_metadata){
    if ((asm_metadata & 0xFFFFFF00u) != 0) return 0;
    return asm_to_vm_primtype((byte)(asm_metadata & 0x7Fu)) != TYPE_NULL;
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

/* Materializes the parsed global rows into the VM heap.
 * Returns 0 on success, -1 when the heap cannot satisfy the allocation
 * (the caller keeps ownership of the row list in that case). */
static inline int allocate_global_heap_block(VM* vm, global_data* last_row, uint32_t total_heap_block_size){
    global_data *current_row = find_genesis_row(last_row);
    int64_t block_address = g_malloc_heap_block(vm, total_heap_block_size, 1);

    if (block_address < 0) {
        vm_error("GLOBAL DATA PARSE", "Not enough VM memory for %u global slots!",
                 (unsigned) total_heap_block_size);
        return -1;
    }

    vm->global_heap_block_address = block_address;
    uint32_t current_mem_slot = vm->global_heap_block_address + 1; // skip the global_heap_block_address since this will describe the heap block size for the GC to handle cleanly

    int row_count = 0;
    while (current_row != NULL) {
        byte prim_metadata = asm_to_vm_metadata(current_row->header);
        word length = current_row->length;

        vm->ram[current_mem_slot++] = (prim_val){
            .metadata = pack_meta(STATE_LOCKED, TYPE_HEAP_BLOCK),
            .data = create_heap_metadata(length, length)
        };

        vm->global_var_table.rows[row_count++] = (global_var_table_row){
            .address = make_prim_val(current_mem_slot, STATE_OPEN, TYPE_REFERENCE) // current_mem_slot must now be = global_heap_block_address + header + 1
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
    return 0;
}


static inline uint32_t get_buffer_next(const uint32_t *buffer, size_t* current_offset){
    return buffer[(*current_offset)++];
}

/* Parses a complete .grr.o image from a word-aligned in-memory buffer.
 *
 * The buffer must be 4-byte aligned and its length must be an exact
 * multiple of 4 (enforced by the shared loader before this is called).
 * Validation is overflow-safe: untrusted lengths are always compared
 * against the bytes/words that remain, never added to them first.
 *
 * Returns 0 on success, -1 when the image is malformed, and -2 when a
 * temporary allocation or the VM heap allocation failed. On failure
 * every temporary allocation owned by the parser is released; the
 * caller keeps ownership of `vm`. */
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

    // Overflow-safe boundary checks: compare the untrusted values against
    // the buffer size / remaining words instead of adding them together.
    if (program_offset < 3 || (size_t)program_offset > buf_word_count) {
        vm_error("GRRVM BYTECODE PARSER", "Invalid bytecode! ERR: 0x2 (program offset %u)",
                 (unsigned) program_offset);
        return -1;
    }

    if ((size_t)vm->program_size > buf_word_count - (size_t)program_offset) {
        vm_error("GRRVM BYTECODE PARSER", "Invalid bytecode! ERR: 0x2 (program length)");
        return -1;
    }

    // vm->program only owns VM_PROGRAM_MAX_SIZE words - refuse oversized
    // programs instead of overflowing the heap buffer on copy/fetch.
    if(vm->program_size > VM_PROGRAM_MAX_SIZE){
        vm_error("GRRVM BYTECODE PARSER", "Program size %u exceeds VM capacity of %u words!",
                 (unsigned) vm->program_size, (unsigned) VM_PROGRAM_MAX_SIZE);
        return -1;
    }

    // Entry point: _program_start is the first word of the _global
    // subroutine inside the loaded program and must address real code.
    if (vm->program_size == 0 || vm->_program_start >= vm->program_size) {
        vm_error("GRRVM BYTECODE PARSER", "Invalid entry point %u (program size %u)!",
                 (unsigned) vm->_program_start, (unsigned) vm->program_size);
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
        size_t region_left = (size_t)program_offset - offset;

        // A record needs at least [header][length]; anything shorter is truncated.
        if (region_left < 2) {
            vm_error("GRRVM BYTECODE PARSER", "Invalid bytecode! ERR: 0x3 (truncated global record)");
            free_global_row(current_row);
            return -1;
        }

        uint32_t asm_metadata = get_buffer_next(buffer, &offset);
        uint32_t length = get_buffer_next(buffer, &offset);

        if (!is_supported_asm_header(asm_metadata)) {
            vm_error("GRRVM BYTECODE PARSER", "Invalid bytecode! ERR: 0x4 (global header 0x%08X)",
                     (unsigned) asm_metadata);
            free_global_row(current_row);
            return -1;
        }

        // create_heap_metadata() packs the length into 16 bits, so larger
        // rows would silently corrupt the heap block header.
        if (length > 0xFFFFu) {
            vm_error("GRRVM BYTECODE PARSER", "Invalid bytecode! ERR: 0x4 (row length %u)",
                     (unsigned) length);
            free_global_row(current_row);
            return -1;
        }

        // Row payload must fit in the words that remain before the program.
        if ((size_t)length > (size_t)program_offset - offset) {
            vm_error("GRRVM BYTECODE PARSER", "Invalid bytecode! ERR: 0x3 (row overruns program)");
            free_global_row(current_row);
            return -1;
        }

        // Accumulate without wrapping and stay inside the VM heap.
        if (length > VM_HEAP_SLOTS || row_count >= VM_HEAP_SLOTS ||
            total_data > VM_HEAP_SLOTS - length) {
            vm_error("GRRVM BYTECODE PARSER", "Invalid bytecode! ERR: 0x5 (global data too large)");
            free_global_row(current_row);
            return -1;
        }

        global_data* new_row = create_global_row(current_row);
        if(new_row == NULL){
            free_global_row(current_row); // release rows accumulated so far
            return -2;
        }
        current_row = new_row;
        row_count++;

        // for arrays
        current_row->header = asm_metadata;
        current_row->length = length;
        current_row->data = malloc(sizeof(uint32_t) * (size_t)length);
        if (length > 0 && current_row->data == NULL) {
            vm_error("GRRVM BYTECODE PARSER", "Out of memory reading global row of %u words!",
                     (unsigned) length);
            free_global_row(current_row);
            return -2;
        }
        total_data += length;

        for(uint32_t i = 0; i < length; i++){
            current_row->data[i] = get_buffer_next(buffer, &offset);
        }
    }

    if (row_count > VM_HEAP_SLOTS - total_data) {
        vm_error("GRRVM BYTECODE PARSER", "Invalid bytecode! ERR: 0x5 (global heap block too large)");
        free_global_row(current_row);
        return -1;
    }
    total_heap_block_size = row_count + total_data;

    // initialize global var table of the VM
    global_var_table *var_table = &vm->global_var_table;
    var_table->row_count = row_count;
    var_table->rows = NULL;
    if (row_count > 0) {
        var_table->rows = malloc(sizeof(global_var_table_row) * (size_t)row_count);
        if (var_table->rows == NULL) {
            vm_error("GRRVM BYTECODE PARSER", "Out of memory building the global var table!");
            free_global_row(current_row);
            var_table->row_count = 0;
            return -2;
        }
    }

    if(total_heap_block_size > 0){
        if (allocate_global_heap_block(vm, current_row, total_heap_block_size) != 0) {
            free(var_table->rows);
            var_table->rows = NULL;
            var_table->row_count = 0;
            return -2;
        }
    }

    return 0;
}

#endif /* GRRVM_PARSER_BYTECODE_PARSE_H */
