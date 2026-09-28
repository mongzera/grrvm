#include "../include/grrvm/hal/init.h"
#include "grrvm/vm_native.h"

int main(int argc, char* argv[]){
    native_registry_init();


    return init(argc, argv);
}
