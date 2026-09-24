#include "../../../include/grrvm/hal/init.h"
#include "../../../include/main_grr.h"

#include "pico/stdlib.h"
void execute(char* src){
    vm_info("[EXECUTE]", "%s\n", src);

    VM* vm = load_vm(&main_grr, main_grr_len);
    vm_start(vm);
}


int init(int argc, char* argv[]){
    stdio_init_all();
    sleep_ms(5000);

    printf("--- GrrVM Starting ---\n");
    printf("Loaded %u bytes from flash\n", main_grr_len);
    VM* vm = load_vm(&main_grr, main_grr_len);
    vm_start(vm);
    // printf("Bytecode location in Flash: %p\n", (void*)program_grro_start);
    // printf("Bytecode size: %u bytes\n", program_grro_length);
}
