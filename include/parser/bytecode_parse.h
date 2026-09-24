#ifndef BYTECODE_PARSE
#define BYTECODE_PARSE

#include "../grrvm/vm.h"
#include "../grrvm/vm_log.h"
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

static inline uint32_t get_buffer_next(const uint32_t *buffer, size_t* current_offset){
    return buffer[(*current_offset)++];
}

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
    size_t mem_offset = 0;

    vm_info("Bytecode Loader", "Offset: %d, Program Offset: %d", offset, program_offset);

    while(offset < program_offset){
        uint32_t asm_metadata = get_buffer_next(buffer, &offset);
        uint32_t length = get_buffer_next(buffer, &offset);
        byte prim_metadata = asm_to_vm_metadata(asm_metadata);


        // for arrays
        if(length > 1){
            vm->ram[mem_offset].metadata = prim_metadata;
            vm->ram[mem_offset].data = length;
            set_val_type(&vm->ram[mem_offset], TYPE_LENGTH);
            mem_offset++;
        }

        for(uint32_t i = 0; i < length; i++){
            vm->ram[mem_offset].metadata = prim_metadata;
            vm->ram[mem_offset].data = get_buffer_next(buffer, &offset);
            mem_offset++;
        }
    }

    return 0;
}

#endif
