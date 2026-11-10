#ifndef GRRVM_PORT_PICO_PROGRAM_STORE_H
#define GRRVM_PORT_PICO_PROGRAM_STORE_H

/* Persistent single-slot guest-program storage for the RP2040.
 *
 * Flash layout - the region is reserved at the TOP of flash, sized from
 * the board's PICO_FLASH_SIZE_BYTES (2 MB on a stock Pico), never from
 * an assumed capacity:
 *
 *   0x000000  firmware (boot2 + .text/.data, ~110 KB today)
 *             ... free ...
 *   base     metadata sector   (4 KiB: magic/version/length/SHA-256/crc)
 *   base+4K  image sectors     (region size - 4 KiB, programmed in
 *                               256-byte pages, erased lazily in 4 KiB
 *                               sectors as the upload progresses)
 *   0x200000 end of flash
 *
 * program_store_init() refuses to operate if the firmware image
 * (__flash_binary_end) would overlap the reserved region, so growing the
 * firmware can never silently corrupt stored programs.
 *
 * Single slot: program_store_begin() erases the metadata sector first,
 * which immediately invalidates any previously stored program. An
 * interrupted upload therefore leaves "no valid program", and the next
 * boot reports that state and accepts a new upload. The firmware area is
 * never erased or written.
 *
 * All erase/program operations run through the Pico SDK's
 * flash_safe_execute() (interrupts disabled, XIP-safe). */

#include <stdbool.h>
#include <stdint.h>

/* Reserved region size; overridable from grr_port_config.h. */
#ifndef GRRVM_PICO_PROGRAM_REGION_SIZE
#define GRRVM_PICO_PROGRAM_REGION_SIZE (128u * 1024u)
#endif

#define GRRVM_PICO_STORE_SECTOR_SIZE 4096u   /* RP2040 flash erase unit */
#define GRRVM_PICO_STORE_PAGE_SIZE   256u    /* RP2040 flash program unit */
#define GRRVM_PICO_STORE_META_SIZE   GRRVM_PICO_STORE_SECTOR_SIZE
#define GRRVM_PICO_STORE_IMAGE_CAPACITY \
    (GRRVM_PICO_PROGRAM_REGION_SIZE - GRRVM_PICO_STORE_META_SIZE)

/* Status codes: GRR_STORE_OK (0) or negative errors. */
#define GRR_STORE_OK         0
#define GRR_STORE_ERR_LAYOUT -1  /* reserved region overlaps firmware/flash */
#define GRR_STORE_ERR_RANGE  -2  /* offset/length outside image capacity */
#define GRR_STORE_ERR_FLASH  -3  /* erase/program operation failed */
#define GRR_STORE_ERR_STATE  -4  /* call sequence violated */

/* Verify the reserved region against the firmware image and load the
 * stored-image metadata. Call once at startup, before any VM work. */
int program_store_init(void);

/* True when the metadata marks a complete, SHA-256-verified image. */
bool program_store_has_valid_image(void);

/* Borrowed XIP-mapped pointer to the stored image and its recorded size.
 * Valid only when program_store_has_valid_image() is true (read-only;
 * no copy into RAM is needed on this port). */
const uint8_t* program_store_image_ptr(void);
uint32_t program_store_image_size(void);

/* --- Storage side of the upload protocol (grr_xfer_ops backend) ----- */

uint32_t program_store_capacity(void);
/* Invalidate the stored image (erase metadata) and reset the write
 * position. Must be called before accepting a new image. */
int program_store_begin(void);
/* Append `len` bytes at image-relative `offset` (sequential). Erases the
 * affected sector on first touch and programs completed 256-byte pages. */
int program_store_write(uint32_t offset, const uint8_t* data, uint32_t len);
/* Program the final partial page (padded with 0xFF). */
int program_store_flush(void);
/* Borrowed XIP pointer to the (partially) written image; length is the
 * number of bytes written so far. */
const uint8_t* program_store_image(uint32_t* len_out);
/* Persist the metadata that marks the image valid. Only call after the
 * stored bytes have been re-read, SHA-256-verified and validated. */
int program_store_commit(const uint8_t sha256[32], uint32_t image_length);
/* Invalidate the stored image (upload cancelled). */
int program_store_invalidate(void);

#endif /* GRRVM_PORT_PICO_PROGRAM_STORE_H */
