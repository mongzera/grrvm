#ifndef VM_TELEMETRY_H
#define VM_TELEMETRY_H

#define MAX_TELEMETRY_ENTRIES 2048

#include <stdint.h>
typedef struct {
    uint32_t timestamp_a;
    uint32_t timestamp_b;

} telemetry_t;

typedef struct {
    float pre_gc_ext_frag;
    float post_gc_ext_frag;

} telemetry_ext_frag;

#endif
