#ifndef GRRVM_VM_ALLOC_H
#define GRRVM_VM_ALLOC_H

/*
 * Heap allocator (Buddy Forest + Slab). Self-contained: needs only prim_val.
 * VM / VM_Thread are forward-declared by struct tag (not typedef) and only
 * used as pointers here, so this header does not need vm.h - vm.h includes
 * this file, not the other way round.
 */

#include "grr_port_config.h"
#include "grrvm/types.h"
#include "grrvm/prim_val.h"

struct VM;
struct VM_Thread;

/* ---------------- Tuning ---------------- */

#ifndef VM_BUDDY_BASE_BLOCK_SLOTS
#define VM_BUDDY_BASE_BLOCK_SLOTS 64u
#endif

#define VM_SLAB_CLASS_COUNT 3u

#ifndef VM_MAX_SLAB_CACHES
/* Hard cap on concurrently-claimed 64-slot slab caches. This is a real
 * resource limit, not just a size hint: once exhausted, g_malloc fails
 * for small allocations even if the BuddyForest still has room. Tune
 * this per-target; it trades static RAM for how many small-object
 * caches can be live at once. */
#define VM_MAX_SLAB_CACHES 64u
#endif

#define VM_NUM_BASE_BLOCKS (VM_HEAP_SLOTS / VM_BUDDY_BASE_BLOCK_SLOTS)
#define VM_MAX_BUDDY_TREES 32u
/* Upper bound on total buddy2 array nodes across all trees:
 * sum over set bits b of (2^(b+1)-1) <= 2*VM_NUM_BASE_BLOCKS. */
#define VM_BUDDY_POOL_SIZE (2u * VM_NUM_BASE_BLOCKS)

#define VM_ALLOC_NONE 0xFFFFFFFFu

extern const word VM_SLAB_CLASS_SIZES[VM_SLAB_CLASS_COUNT]; /* {8, 16, 32} */

/* ---------------- Buddy Forest ----------------
 * Classic single-array "buddy2" layout: longest[i] holds (order+1) of the
 * largest free block reachable under node i, 0 = fully allocated.
 * Allocation descends toward a child that can satisfy the request, then
 * re-maxes every ancestor. Freeing sets the node, then walks up: a parent
 * is promoted to its own full capacity only when BOTH children are at
 * full capacity (both buddies entirely free); otherwise it takes the max
 * of its children. Plain max() alone is not enough to detect a merge.
 * Needs no per-block free-list pointers and no separate free bitmap.
 */
typedef struct BuddyTree {
    word base_slot;  /* absolute slot index into vm->ram[] where this tree begins */
    byte order;      /* tree spans 2^order base blocks (2^order * VM_BUDDY_BASE_BLOCK_SLOTS slots) */
    byte *longest;   /* slice into BuddyForest.pool, length 2^(order+1)-1 */
} BuddyTree;

typedef struct BuddyForest {
    BuddyTree trees[VM_MAX_BUDDY_TREES];
    byte num_trees;
    word free_slots;  /* running total of free slots across all trees */
    byte pool[VM_BUDDY_POOL_SIZE];
} BuddyForest;

/* ---------------- Slab Allocator ----------------
 * Each SlabCache is exactly one buddy base block (VM_BUDDY_BASE_BLOCK_SLOTS
 * slots), sliced into equal-size sub-blocks and threaded as an intrusive
 * free list - a free sub-block's own `data` field holds the next free
 * slot index, so no separate free-list storage is needed either.
 */
typedef struct SlabCache {
    word base_slot;
    word free_list_head; /* VM_ALLOC_NONE when full */
    word next_available;  /* next cache in this class's "has room" list, or VM_ALLOC_NONE */
    half used_count;
    half class_index;     /* index into VM_SLAB_CLASS_SIZES */
} SlabCache;

typedef struct SlabAllocator {
    SlabCache caches[VM_MAX_SLAB_CACHES];
    byte cache_in_use[VM_MAX_SLAB_CACHES];
    word available_head[VM_SLAB_CLASS_COUNT]; /* index into caches[], or VM_ALLOC_NONE */
} SlabAllocator;

typedef struct VM_Allocator {
    BuddyForest buddy;
    SlabAllocator slab;
    /* base_block_owner[i]: VM_ALLOC_NONE if base block i currently belongs
     * to the buddy forest (free, or part of a >=64-slot allocation);
     * otherwise the index into slab.caches[] that owns it. O(1) routing
     * for g_free on small allocations. */
    word base_block_owner[VM_NUM_BASE_BLOCKS];
} VM_Allocator;

/* ---------------- Heap statistics ---------------- */

/* Both fields are in slots and count the whole heap (buddy + slab).
 *  total_free_slots  - every free slot: free buddy blocks plus free
 *                      sub-blocks sitting in partially used slab caches.
 *  longest_free_block - size (header included) of the largest single
 *                      block g_malloc could hand out right now. The
 *                      largest usable payload is this minus 1.
 * External fragmentation, e.g.: 1.0f - (float)longest / (float)total. */
typedef struct VM_HeapStats {
    word buddy_free_slots;
    word longest_buddy_block;
} VM_HeapStats;

/* ---------------- Public API ---------------- */

/* Must be called once at VM startup, before any g_malloc/g_free. */
void vm_alloc_init(struct VM *vm);

/* Allocates n_slots usable slots. Physically reserves n_slots+1 (a
 * TYPE_LENGTH header slot immediately precedes the data). Returns a
 * TYPE_REFERENCE prim_val pointing at the first usable slot - this value
 * is meant to live on the stack (op_stack / call_stack_frame), not in
 * ram[]. On exhaustion, logs via vm_error and returns a GARBAGE prim_val;
 * caller must check get_prim_state()/get_prim_type() before use. */
prim_val g_malloc(struct VM_Thread *thread, word n_slots);
prim_val g_malloc_direct(struct VM *vm, word n_slots, byte print_error);

/* Frees a block previously returned by g_malloc. reference must be the
 * exact TYPE_REFERENCE value g_malloc returned (its .data is the slot
 * index right after the header). Reads the header for the block size,
 * so no bookkeeping beyond the reference itself is required. */
void g_free(struct VM_Thread *, prim_val reference);
void g_free_direct(struct VM *, word slot_index);

/* O(#trees + #slab caches), no tree walking. Safe to call at any time. */
void vm_heap_stats(const struct VM *vm, VM_HeapStats *out);

#endif /* GRRVM_VM_ALLOC_H */
