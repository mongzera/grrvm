#include <stdio.h>
#include <string.h>
#include "../include/grrvm/vm_log.h"

void print_command_unrecognized(){
    printf("Error: Command is unrecognized!\n");
}

void print_help(){
    printf("Welcome!\nGRRVM Help Directory:\n==========================\nTo compile: ./grrvm -c [filename].grr | ./grrvm --compile [filename].grr\nTo execute: ./grrvm -r [filename].grro | ./grrvm --run [filename].grro\n");
}

void compile(char* src){
    vm_info("COMPILE", "%s\n", src);
}

void execute(char* src){
    vm_info("EXECUTE", "%s\n", src);
}

int main(int argc, char* argv[]){
    if(argc == 1){
        print_help();
    }
    else if(argc == 3){
        if(strcmp(argv[1], "-c") == 0 || strcmp(argv[1], "--compile") == 0)     compile(argv[2]);
        else if(strcmp(argv[1], "-r") == 0 || strcmp(argv[1], "--run") == 0)    execute(argv[2]);
        else print_help();
    }
    else{
        print_command_unrecognized();
    }

    return 0;
}
