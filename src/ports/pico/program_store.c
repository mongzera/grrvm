#include "program_store.h"

#include "grrvm/program_transfer.h" /* grr_crc32 + LE helpers */
#include "hardware/flash.h"
#include "pico/flash.h"
#include "pico/stdlib.h"
#include <string.h>

/* Metadata layout inside the first page of the metadata sector.
 * Everything little-endian; explicit byte access, no packed structs. */
#define STORE_MAGIC        0x50525247u  /* "GRRP" */
#define STORE_META_VERSION 1u
#define STORE_META_LEN     48u

/* Reserved region geometry (flash offsets, relative to start of flash). */
static uint32_t s_region_base;  /* metadata sector */
static uint32_t s_image_base;   /* image sectors = region_base + 4 KiB */

/* Loaded metadata */
static bool     s_valid;
static uint32_t s_image_len;

/* Upload session state (RAM only) */
static uint32_t s_write_pos;   /* image bytes handed to write() so far */
static uint32_t s_erased_upto; /* image area erased up to (sector steps) */
static uint32_t s_buffered;    /* bytes staged in s_page_buf */
static uint8_t  s_page_buf[GRRVM_PICO_STORE_PAGE_SIZE];

/* Firmware end symbol provided by the Pico linker script. */
extern const char __flash_binary_end[];

/* -------------------------------------------------------------------------
 * XIP-mapped addresses
 * ------------------------------------------------------------------------- */

static const uint8_t* xip_ptr(uint32_t flash_offset) {
    return (const uint8_t*)(XIP_BASE + flash_offset);
}

/* -------------------------------------------------------------------------
 * Flash operations through flash_safe_execute()
 *
 * Each callback runs with interrupts disabled and only touches RAM plus
 * the SDK's RAM-resident flash_range_* routines, which is exactly the
 * contract required while XIP is suspended. Erase/program of one
 * sector/page is short (~45 ms worst case for a 4 KiB erase), and the
 * gaps between operations let the USB stack keep servicing the host.
 * ------------------------------------------------------------------------- */

typedef struct {
    uint32_t offset;      /* flash offset */
    const uint8_t* data;  /* program source (RAM) */
} flash_op_t;

static int flash_erase_callback(void* param) {
    const flash_op_t* op = (const flash_op_t*)param;
    flash_range_erase(op->offset, GRRVM_PICO_STORE_SECTOR_SIZE);
    return 0;
}

static int flash_program_callback(void* param) {
    const flash_op_t* op = (const flash_op_t*)param;
    flash_range_program(op->offset, op->data, GRRVM_PICO_STORE_PAGE_SIZE);
    return 0;
}

static int store_erase_sector(uint32_t flash_offset) {
    flash_op_t op = { .offset = flash_offset, .data = NULL };
    if (flash_safe_execute(flash_erase_callback, &op, 1000) != PICO_OK) {
        return GRR_STORE_ERR_FLASH;
    }
    return GRR_STORE_OK;
}

static int store_program_page(uint32_t flash_offset, const uint8_t* data) {
    flash_op_t op = { .offset = flash_offset, .data = data };
    if (flash_safe_execute(flash_program_callback, &op, 1000) != PICO_OK) {
        return GRR_STORE_ERR_FLASH;
    }
    return GRR_STORE_OK;
}

/* -------------------------------------------------------------------------
 * Startup / validity
 * ------------------------------------------------------------------------- */

int program_store_init(void) {
    s_region_base = PICO_FLASH_SIZE_BYTES - GRRVM_PICO_PROGRAM_REGION_SIZE;
    s_image_base = s_region_base + GRRVM_PICO_STORE_META_SIZE;
    s_valid = false;
    s_image_len = 0;
    s_write_pos = 0;
    s_erased_upto = 0;
    s_buffered = 0;

    /* The firmware (boot2 + code + constants + boot metadata) must not
     * reach into the reserved region; refuse to run if it does. */
    uintptr_t firmware_end = (uintptr_t)__flash_binary_end;
    uintptr_t region_start = (uintptr_t)(XIP_BASE + s_region_base);
    if (firmware_end > region_start) {
        return GRR_STORE_ERR_LAYOUT;
    }

    if (GRRVM_PICO_PROGRAM_REGION_SIZE < 2u * GRRVM_PICO_STORE_SECTOR_SIZE
        || (GRRVM_PICO_STORE_IMAGE_CAPACITY % GRRVM_PICO_STORE_SECTOR_SIZE) != 0
        || (GRRVM_PICO_STORE_IMAGE_CAPACITY % GRRVM_PICO_STORE_PAGE_SIZE) != 0) {
        return GRR_STORE_ERR_LAYOUT;
    }

    /* Read back and check the stored metadata (XIP reads are safe - no
     * flash operation is in progress here). */
    const uint8_t* meta = xip_ptr(s_region_base);
    uint32_t magic = grr_xfer_get_u32(meta);
    uint32_t version = grr_xfer_get_u32(meta + 4);
    uint32_t length = grr_xfer_get_u32(meta + 8);
    uint32_t crc = grr_xfer_get_u32(meta + 44);

    if (magic == STORE_MAGIC
        && version == STORE_META_VERSION
        && length > 0
        && (length % sizeof(uint32_t)) == 0
        && length <= GRRVM_PICO_STORE_IMAGE_CAPACITY
        && grr_crc32(meta, 44) == crc) {
        s_valid = true;
        s_image_len = length;
    }

    return GRR_STORE_OK;
}

bool program_store_has_valid_image(void) {
    return s_valid;
}

const uint8_t* program_store_image_ptr(void) {
    return xip_ptr(s_image_base);
}

uint32_t program_store_image_size(void) {
    return s_image_len;
}

/* -------------------------------------------------------------------------
 * Upload-session storage operations
 * ------------------------------------------------------------------------- */

uint32_t program_store_capacity(void) {
    return GRRVM_PICO_STORE_IMAGE_CAPACITY;
}

int program_store_begin(void) {
    /* Invalidate first: if anything interrupts the upload from here on,
     * the next boot finds an erased metadata sector and comes up in
     * no-program/upload mode instead of executing a partial image. */
    s_valid = false;
    s_image_len = 0;
    s_write_pos = 0;
    s_erased_upto = 0;
    s_buffered = 0;

    return store_erase_sector(s_region_base);
}

int program_store_write(uint32_t offset, const uint8_t* data, uint32_t len) {
    if (data == NULL || len == 0 || offset != s_write_pos) {
        return GRR_STORE_ERR_STATE;
    }
    if (offset + len > GRRVM_PICO_STORE_IMAGE_CAPACITY || offset + len < offset) {
        return GRR_STORE_ERR_RANGE;
    }

    uint32_t end = offset + len;

    /* Erase every sector the incoming bytes touch, ahead of programming
     * them (flash can only clear bits; a sector must be erased first). */
    while (s_erased_upto < end) {
        if (store_erase_sector(s_image_base + s_erased_upto) != GRR_STORE_OK) {
            return GRR_STORE_ERR_FLASH;
        }
        s_erased_upto += GRRVM_PICO_STORE_SECTOR_SIZE;
    }

    /* Stage bytes into 256-byte page buffers and program each full page. */
    for (uint32_t i = 0; i < len; i++) {
        s_page_buf[s_buffered++] = data[i];

        if (s_buffered == GRRVM_PICO_STORE_PAGE_SIZE) {
            uint32_t page_offset = offset + i + 1u - GRRVM_PICO_STORE_PAGE_SIZE;
            if (store_program_page(s_image_base + page_offset, s_page_buf) != GRR_STORE_OK) {
                /* Leave s_write_pos untouched for the tail that was not
                 * staged; the transfer reports ERR_FLASH to the host. */
                s_buffered = 0;
                return GRR_STORE_ERR_FLASH;
            }
            s_buffered = 0;
        }
    }

    s_write_pos = end;
    return GRR_STORE_OK;
}

int program_store_flush(void) {
    if (s_buffered == 0) {
        return GRR_STORE_OK;
    }

    memset(s_page_buf + s_buffered, 0xFF, GRRVM_PICO_STORE_PAGE_SIZE - s_buffered);

    uint32_t page_offset = s_write_pos - s_buffered;
    s_buffered = 0;

    if (store_program_page(s_image_base + page_offset, s_page_buf) != GRR_STORE_OK) {
        return GRR_STORE_ERR_FLASH;
    }
    return GRR_STORE_OK;
}

const uint8_t* program_store_image(uint32_t* len_out) {
    if (len_out != NULL) {
        *len_out = s_write_pos;
    }
    return xip_ptr(s_image_base);
}

int program_store_commit(const uint8_t sha256[32], uint32_t image_length) {
    if (sha256 == NULL
        || image_length == 0
        || image_length > GRRVM_PICO_STORE_IMAGE_CAPACITY
        || (image_length % sizeof(uint32_t)) != 0) {
        return GRR_STORE_ERR_RANGE;
    }

    uint8_t page[GRRVM_PICO_STORE_PAGE_SIZE];
    memset(page, 0xFF, sizeof(page));

    grr_xfer_put_u32(page + 0, STORE_MAGIC);
    grr_xfer_put_u32(page + 4, STORE_META_VERSION);
    grr_xfer_put_u32(page + 8, image_length);
    memcpy(page + 12, sha256, 32);
    grr_xfer_put_u32(page + 44, grr_crc32(page, 44));

    /* The metadata sector was erased in program_store_begin(); this
     * single page program is what finally marks the image valid. */
    if (store_program_page(s_region_base, page) != GRR_STORE_OK) {
        return GRR_STORE_ERR_FLASH;
    }

    s_valid = true;
    s_image_len = image_length;
    return GRR_STORE_OK;
}

int program_store_invalidate(void) {
    s_valid = false;
    s_image_len = 0;
    return store_erase_sector(s_region_base);
}
