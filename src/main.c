#include <stdio.h>
#include <string.h>
#include "../include/grrvm/vm_log.h"
#include "grrvm/vm.h"
#include "grrvm/vm_program_loader.h"

void print_command_unrecognized(void){
    printf("Error: Command is unrecognized!\n");
}

void print_help(void){
    const int MSG_COUNT = 6;
    const char* message[MSG_COUNT];
    message[0] = "Welcome!";
    message[1] = "GRRVM Help Directory:";
    message[2] = "==========================";
    message[3] = "To compile: ./grrvm -c [filename].grr | ./grrvm --compile [filename].grr";
    message[4] = "To execute: ./grrvm -r [filename].grro | ./grrvm --run [filename].grro";
    message[5] = "To playground: ./grrvm -p | ./grrvm --playground";

    for (int i = 0; i < MSG_COUNT; i++) {
        printf("%s\n", message[i]);
    }
}

void compile(char* src){
    vm_info("[COMPILE]", "%s\n", src);
}

void execute(char* src){
    vm_info("[EXECUTE]", "%s\n", src);
    VM* vm = 0;
    vm_program_loader(src, vm);
    vm_start(vm);
}

void playground(void){
    vm_info("[PLAYGROUND ENVIRONMENT]%s", "\n");
}


int main(int argc, char* argv[]){
    if(argc == 1){
        print_help();
    }
    else if(argc == 3){
        if(strcmp(argv[1], "-c") == 0 || strcmp(argv[1], "--compile") == 0)     compile(argv[2]);
        else if(strcmp(argv[1], "-r") == 0 || strcmp(argv[1], "--run") == 0)    execute(argv[2]);
        else if(strcmp(argv[1], "-p") == 0 || strcmp(argv[1], "--playground") == 0)    playground();
        else print_help();
    }
    else{
        print_command_unrecognized();
    }

    return 0;
}
