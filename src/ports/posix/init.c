#include "grrvm/hal/init.h"
#include "grrvm/hal/load_vm.h"
#include "grrvm/hal/vm_hal_math.h"
#include "grrvm/debug/run_tests.h"
#include "grrvm/vm.h"
#include "grrvm/vm_native.h"
#include <stdlib.h>
#include <string.h>


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
    VM* vm = load_vm_from_file(src);
    vm_start(vm);
    vm_terminate(vm);
}

/* Removes -t/--test from argv (wherever it appears) and reports whether it
 * was present, so the tests can run first and the remaining arguments are
 * handled exactly as they would be without the flag. */
static int strip_test_flag(int argc, char* argv[], int* requested){
    int kept = 1; /* argv[0] is the program name */

    *requested = 0;

    for(int i = 1; i < argc; i++){
        if(strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--test") == 0){
            *requested = 1;
            continue;
        }
        argv[kept++] = argv[i];
    }

    argv[kept] = NULL;
    return kept;
}

int init(int argc, char* argv[]){

    // initialize HAL functions here
    hal_rng_init(0);

    native_registry_init();

    int tests_requested = 0;
    argc = strip_test_flag(argc, argv, &tests_requested);

    if(tests_requested){
#ifdef GRRVM_BUILD_TESTS
        int failures = run_all_tests();
        if(failures != 0){
            printf("[GRRVM] System checks failed (%d); aborting.\n", failures);
            return 1;
        }
        printf("[GRRVM] System checks passed; continuing.\n");
#else
        printf("[GRRVM] This build has no tests compiled in "
               "(configure with -DGRRVM_BUILD_TESTS=ON).\n");
        return 1;
#endif
    }

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
