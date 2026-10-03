#include "grrvm/vm_dump.h"
#include "grrvm/common/vm_dump_common.h"
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#define CREATE_DIR(path) mkdir(path, 0777)


void dump_vm_heap_terminal(const VM *vm) {
    if (!vm) {
        printf("[VM Dump Error] Invalid VM pointer.\n");
        return;
    }

    printf("VM HEAP DUMP (TERMINAL)\n");
    printf("Heap Slots: %d\n\n", (int)VM_HEAP_SLOTS);
    dump_heap_to_stream(stdout, vm->ram, VM_HEAP_SLOTS);
}

bool dump_vm_heap_file(const VM *vm) {
    if (!vm) {
        printf("[VM Dump Error] Invalid VM pointer.\n");
        return false;
    }

    CREATE_DIR("dump");

    time_t t = time(NULL);
    struct tm tm_info = *localtime(&t);

    char filepath[256];
    snprintf(filepath, sizeof(filepath), "dump/%04d-%02d-%02d_%02d-%02d-%02d_heap-dump.txt",
             tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday,
             tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);

    FILE *file = fopen(filepath, "w");
    if (!file) {
        snprintf(filepath, sizeof(filepath), "./%04d-%02d-%02d_%02d-%02d-%02d_heap-dump.txt",
                 tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday,
                 tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);
        file = fopen(filepath, "w");

        if (!file) {
            printf("[VM Dump Error] Failed to create dump file. Falling back to terminal:\n");
            dump_vm_heap_terminal(vm);
            return false;
        }
    }

    fprintf(file, "VM HEAP DUMP LOG\n");
    fprintf(file, "Timestamp: %04d-%02d-%02d %02d:%02d:%02d\n",
            tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday,
            tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);
    fprintf(file, "Heap Slots: %d\n\n", (int)VM_HEAP_SLOTS);

    dump_heap_to_stream(file, vm->ram, VM_HEAP_SLOTS);

    fclose(file);
    printf("Successfully written VM heap dump to %s\n", filepath);
    return true;
}
