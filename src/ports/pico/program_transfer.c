#include "program_transfer.h"

#include "grrvm/hal/load_vm.h"
#include "grrvm/program_transfer.h"
#include "grrvm/vm_host_poll.h"
#include "program_store.h"
#include "pico/stdlib.h"
#include <stdio.h>

static grr_xfer_engine s_engine;
static VM* s_pending_vm;      /* built during final validation */
static int s_image_ready;     /* a verified image was installed */

/* -------------------------------------------------------------------------
 * grr_xfer_ops backend: program_store + stdio responses
 * ------------------------------------------------------------------------- */

static uint32_t xfer_capacity(void* ctx) {
    (void)ctx;
    return program_store_capacity();
}

static int xfer_begin(void* ctx, const grr_xfer_header* header) {
    (void)ctx;
    (void)header;
    /* A leftover VM from an abandoned transfer must not leak. */
    if (s_pending_vm != NULL) {
        vm_terminate(s_pending_vm);
        s_pending_vm = NULL;
    }
    return program_store_begin();
}

static int xfer_write(void* ctx, uint32_t offset, const uint8_t* data, uint32_t len) {
    (void)ctx;
    return program_store_write(offset, data, len);
}

static int xfer_flush(void* ctx) {
    (void)ctx;
    return program_store_flush();
}

static const uint8_t* xfer_image(void* ctx, uint32_t* len_out) {
    (void)ctx;
    return program_store_image(len_out);
}

static int xfer_validate(void* ctx, const uint8_t* image, uint32_t len) {
    (void)ctx;

    if (s_pending_vm != NULL) {
        vm_terminate(s_pending_vm);
        s_pending_vm = NULL;
    }

    int error = GRR_LOAD_OK;
    VM* vm = NULL;
    if (load_vm_from_bytes_ex(image, len, &vm, &error) != 0 || vm == NULL) {
        return (error == GRR_LOAD_ERR_MEMORY)
             ? GRR_XFER_ERR_VM_MEMORY
             : GRR_XFER_ERR_STRUCTURE;
    }

    /* The image is structurally valid: keep the ready-to-run VM so the
     * startup flow can execute it without parsing the flash twice. */
    s_pending_vm = vm;
    return 0;
}

static int xfer_commit(void* ctx, const grr_xfer_header* header) {
    (void)ctx;
    return program_store_commit(header->sha256, header->image_length);
}

static int xfer_abort(void* ctx) {
    (void)ctx;
    return program_store_invalidate();
}

static void xfer_emit(void* ctx, const uint8_t* frame, size_t length) {
    (void)ctx;
    fwrite(frame, 1, length, stdout);
    fflush(stdout);
}

static const grr_xfer_ops s_ops = {
    .ctx = NULL,
    .capacity = xfer_capacity,
    .begin = xfer_begin,
    .write = xfer_write,
    .flush = xfer_flush,
    .image = xfer_image,
    .validate = xfer_validate,
    .commit = xfer_commit,
    .abort = xfer_abort,
    .emit = xfer_emit,
};

/* -------------------------------------------------------------------------
 * Byte intake
 * ------------------------------------------------------------------------- */

int pico_transfer_poll(void) {
    grr_xfer_state state = grr_xfer_state_of(&s_engine);

    /* Past HEADER_OK the storage may be touched: only the service loop
     * (VM stopped) may feed the engine from here. */
    if (state == GRR_XFER_STATE_HEADER_OK
        || state == GRR_XFER_STATE_RECEIVING
        || state == GRR_XFER_STATE_DONE) {
        return 1;
    }

    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        grr_xfer_feed(&s_engine, (uint8_t)c);
        if (grr_xfer_state_of(&s_engine) != GRR_XFER_STATE_IDLE) {
            break;
        }
    }

    return grr_xfer_state_of(&s_engine) != GRR_XFER_STATE_IDLE;
}

void pico_transfer_service(void) {
    for (;;) {
        if (grr_xfer_state_of(&s_engine) == GRR_XFER_STATE_HEADER_OK) {
            /* Safe point: no VM is running, storage may be erased. */
            grr_xfer_accept(&s_engine);
        }

        if (grr_xfer_state_of(&s_engine) == GRR_XFER_STATE_DONE) {
            s_image_ready = 1;
            grr_xfer_reset(&s_engine);
            return;
        }

        int c = getchar_timeout_us(100 * 1000); /* 100 ms idle slice */
        if (c != PICO_ERROR_TIMEOUT) {
            grr_xfer_feed(&s_engine, (uint8_t)c);
        }
    }
}

int pico_transfer_header_pending(void) {
    return grr_xfer_state_of(&s_engine) == GRR_XFER_STATE_HEADER_OK;
}

int pico_transfer_take_ready(void) {
    int ready = s_image_ready;
    s_image_ready = 0;
    return ready;
}

VM* pico_transfer_take_vm(void) {
    VM* vm = s_pending_vm;
    s_pending_vm = NULL;
    return vm;
}

/* -------------------------------------------------------------------------
 * Host-poll hook: lets an upload interrupt a running guest program.
 * Strong definition overriding the weak default in src/core/vm.c.
 * ------------------------------------------------------------------------- */

int vm_host_poll(void) {
    return pico_transfer_poll();
}

/* -------------------------------------------------------------------------
 * Boot-time driver initialization
 * ------------------------------------------------------------------------- */

void pico_transfer_init(void) {
    grr_xfer_init(&s_engine, &s_ops);
    s_pending_vm = NULL;
    s_image_ready = 0;
}
