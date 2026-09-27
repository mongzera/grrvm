#include "grrvm/types.h"
#include "grrvm/vm.h"
#include "grrvm/vm_gc.h"
#include "grrvm/vm_mem.h"
#include "grrvm/vm_alloc.h"
#include "grrvm/vm_thread.h"
#include <string.h>

const word VM_SLAB_CLASS_SIZES[VM_SLAB_CLASS_COUNT] = { 8u, 16u, 32u };

/* =======================================================================
 * BuddyTree - single-array "buddy2" allocator
 * ======================================================================= */

static word buddy_tree_alloc(BuddyTree *tree, byte want_order) {
    if (tree->longest[0] < (byte)(want_order + 1)) return VM_ALLOC_NONE;

    word index = 0;
    byte node_order = tree->order;
    while (node_order != want_order) {
        word left = index * 2 + 1;
        word right = index * 2 + 2;
        index = (tree->longest[left] >= (byte)(want_order + 1)) ? left : right;
        node_order--;
    }
    tree->longest[index] = 0;

    word level = (word)(tree->order - node_order);
    word level_start = (1u << level) - 1u;
    word offset_in_level = index - level_start;
    word node_offset = offset_in_level << want_order; /* in base blocks */

    while (index != 0) {
        index = (index - 1) / 2;
        byte l = tree->longest[index * 2 + 1];
        byte r = tree->longest[index * 2 + 2];
        tree->longest[index] = (l > r) ? l : r;
    }

    return tree->base_slot + node_offset * VM_BUDDY_BASE_BLOCK_SLOTS;
}

static void buddy_tree_free(BuddyTree *tree, word node_offset, byte order) {
    word level = (word)(tree->order - order);
    word level_start = (1u << level) - 1u;
    word index = level_start + (node_offset >> order);

    tree->longest[index] = (byte)(order + 1);

    /* Walking up: a parent only gets promoted to its own full capacity
     * when BOTH children are themselves at full capacity (i.e. nothing
     * anywhere under either child is allocated) - that's what "the
     * buddy is free" actually means. Otherwise the parent is just
     * whichever child currently offers the bigger free block. */
    byte child_order = order;
    while (index != 0) {
        word parent = (index - 1) / 2;
        byte l = tree->longest[parent * 2 + 1];
        byte r = tree->longest[parent * 2 + 2];
        byte child_full = (byte)(child_order + 1);
        if (l == child_full && r == child_full) {
            tree->longest[parent] = (byte)(child_full + 1);
        } else {
            tree->longest[parent] = (l > r) ? l : r;
        }
        index = parent;
        child_order++;
    }
}

static word buddy_alloc(BuddyForest *forest, byte want_order) {
    for (byte i = 0; i < forest->num_trees; i++) {
        if (forest->trees[i].order >= want_order) {
            word addr = buddy_tree_alloc(&forest->trees[i], want_order);
            if (addr != VM_ALLOC_NONE) return addr;
        }
    }
    return VM_ALLOC_NONE; /* forest exhausted at this order */
}

static void buddy_free(BuddyForest *forest, word addr, byte order) {
    for (byte i = 0; i < forest->num_trees; i++) {
        BuddyTree *t = &forest->trees[i];
        word tree_span = (word)VM_BUDDY_BASE_BLOCK_SLOTS << t->order;
        if (addr >= t->base_slot && addr < t->base_slot + tree_span) {
            word node_offset = (addr - t->base_slot) / VM_BUDDY_BASE_BLOCK_SLOTS;
            buddy_tree_free(t, node_offset, order);
            return;
        }
    }
    vm_error("HEAP", "buddy_free: address 0x%X does not belong to any tree!", addr);
}

/* =======================================================================
 * Slab allocator
 * ======================================================================= */

static byte slab_acquire_descriptor(SlabAllocator *s) {
    for (word i = 0; i < VM_MAX_SLAB_CACHES; i++) {
        if (!s->cache_in_use[i]) {
            s->cache_in_use[i] = 1;
            return (byte)i;
        }
    }
    return 0xFF;
}

static word slab_alloc(VM *vm, byte class_index) {
    SlabAllocator *s = &vm->allocator.slab;
    word cache_idx = s->available_head[class_index];

    if (cache_idx == VM_ALLOC_NONE) {
        word base = buddy_alloc(&vm->allocator.buddy, 0); /* one base block */
        if (base == VM_ALLOC_NONE) return VM_ALLOC_NONE;

        byte desc = slab_acquire_descriptor(s);
        if (desc == 0xFF) {
            buddy_free(&vm->allocator.buddy, base, 0);
            vm_error("HEAP", "Slab descriptor pool exhausted (VM_MAX_SLAB_CACHES=%u)!", VM_MAX_SLAB_CACHES);
            return VM_ALLOC_NONE;
        }

        SlabCache *sc = &s->caches[desc];
        word class_size = VM_SLAB_CLASS_SIZES[class_index];
        word count = VM_BUDDY_BASE_BLOCK_SLOTS / class_size;

        sc->base_slot = base;
        sc->class_index = class_index;
        sc->used_count = 0;
        sc->free_list_head = base;
        for (word i = 0; i < count; i++) {
            word slot = base + i * class_size;
            vm->ram[slot].data = (i + 1 < count) ? (base + (i + 1) * class_size) : VM_ALLOC_NONE;
        }
        vm->allocator.base_block_owner[base / VM_BUDDY_BASE_BLOCK_SLOTS] = desc;

        sc->next_available = s->available_head[class_index];
        s->available_head[class_index] = desc;
        cache_idx = desc;
    }

    SlabCache *sc = &s->caches[cache_idx];
    word addr = sc->free_list_head;
    sc->free_list_head = vm->ram[addr].data;
    sc->used_count++;

    if (sc->free_list_head == VM_ALLOC_NONE) {
        /* cache is now full - drop it from the "has room" list */
        s->available_head[class_index] = sc->next_available;
    }
    return addr;
}

static void slab_free(VM *vm, word addr) {
    VM_Allocator *a = &vm->allocator;
    word base_block = addr / VM_BUDDY_BASE_BLOCK_SLOTS;
    word desc = a->base_block_owner[base_block];

    if (desc == VM_ALLOC_NONE || desc >= VM_MAX_SLAB_CACHES) {
        vm_error("HEAP", "slab_free: address 0x%X is not part of any slab cache!", addr);
        return;
    }

    SlabCache *sc = &a->slab.caches[desc];
    byte class_index = (byte)sc->class_index;
    byte was_full = (sc->free_list_head == VM_ALLOC_NONE);

    vm->ram[addr].data = sc->free_list_head;
    sc->free_list_head = addr;
    sc->used_count--;

    if (was_full) {
        sc->next_available = a->slab.available_head[class_index];
        a->slab.available_head[class_index] = desc;
    }

    if (sc->used_count == 0) {
        /* cache is empty - unlink it from the available list and hand the
         * base block back to the BuddyForest */
        word *cursor = &a->slab.available_head[class_index];
        while (*cursor != VM_ALLOC_NONE) {
            if (*cursor == desc) { *cursor = sc->next_available; break; }
            cursor = &a->slab.caches[*cursor].next_available;
        }
        buddy_free(&a->buddy, sc->base_slot, 0);
        a->base_block_owner[base_block] = VM_ALLOC_NONE;
        a->slab.cache_in_use[desc] = 0;
    }
}

/* =======================================================================
 * vm_alloc_init - greedy binary decomposition of VM_NUM_BASE_BLOCKS
 * into power-of-two BuddyTrees (handles heaps that aren't themselves
 * a power of two, e.g. Pico's 25600-slot heap -> trees of order 8/7/4).
 * ======================================================================= */

void vm_alloc_init(VM *vm) {
    //set HEAP all to 0;
    memset(vm->ram, 0, sizeof(vm->ram));

    VM_Allocator *a = &vm->allocator;

    for (word i = 0; i < VM_NUM_BASE_BLOCKS; i++) a->base_block_owner[i] = VM_ALLOC_NONE;
    for (word i = 0; i < VM_MAX_SLAB_CACHES; i++) a->slab.cache_in_use[i] = 0;
    for (word c = 0; c < VM_SLAB_CLASS_COUNT; c++) a->slab.available_head[c] = VM_ALLOC_NONE;

    a->buddy.num_trees = 0;
    word remaining = VM_NUM_BASE_BLOCKS;
    word base_block_cursor = 0;
    word pool_offset = 0;

    for (int bit = 31; bit >= 0 && remaining > 0; bit--) {
        word block_count = 1u << bit;
        if (!(remaining & block_count)) continue;

        BuddyTree *t = &a->buddy.trees[a->buddy.num_trees++];
        t->base_slot = base_block_cursor * VM_BUDDY_BASE_BLOCK_SLOTS;
        t->order = (byte)bit;
        t->longest = &a->buddy.pool[pool_offset];
        pool_offset += (1u << (bit + 1)) - 1u;

        word idx = 0;
        for (byte level = 0; level <= t->order; level++) {
            word count = 1u << level;
            byte val = (byte)(t->order - level + 1);
            for (word j = 0; j < count; j++) t->longest[idx++] = val;
        }

        base_block_cursor += block_count;
        remaining -= block_count;
    }
}

/* =======================================================================
 * Public API
 * ======================================================================= */

static prim_val finish_alloc(VM *vm, word addr, word block_size, word n_slots) {
    vm->ram[addr] = make_prim_val(block_size, STATE_LOCKED, TYPE_LENGTH);
    for (word i = 1; i <= n_slots; i++) {
        vm->ram[addr + i] = make_prim_val(0, STATE_OPEN, TYPE_NULL);
    }
    return make_prim_val(addr + 1, STATE_OPEN, TYPE_REFERENCE);
}

prim_val g_malloc_direct(VM *vm, word n_slots, byte print_error) {

    word total_needed = n_slots + 1; /* + header */
    if (total_needed <= 32) {
        byte class_index = 0xFF;
        for (byte c = 0; c < VM_SLAB_CLASS_COUNT; c++) {
            if (total_needed <= VM_SLAB_CLASS_SIZES[c]) { class_index = c; break; }
        }
        word addr = slab_alloc(vm, class_index);
        if (addr == VM_ALLOC_NONE) {
            if (print_error) vm_error("HEAP", "Out of memory (slab class %u slots)", VM_SLAB_CLASS_SIZES[class_index]);
            return make_prim_val(0, STATE_OPEN, TYPE_NULL);
        }
        return finish_alloc(vm, addr, VM_SLAB_CLASS_SIZES[class_index], n_slots);
    } else {
        byte want_order = 0;
        while (((word)VM_BUDDY_BASE_BLOCK_SLOTS << want_order) < total_needed) want_order++;
        word addr = buddy_alloc(&vm->allocator.buddy, want_order);
        if (addr == VM_ALLOC_NONE) {
            if (print_error) vm_error("HEAP", "Out of memory (buddy order %u)", want_order);
            return make_prim_val(0, STATE_OPEN, TYPE_NULL);
        }
        word block_size = (word)VM_BUDDY_BASE_BLOCK_SLOTS << want_order;
        return finish_alloc(vm, addr, block_size, n_slots);
    }
}

/*
 * Automatically runs garbage collection if the allocation fails.
 */
prim_val g_malloc(VM_Thread *thread, word n_slots) {
    VM* vm = thread->vm;
    prim_val reference = g_malloc_direct(vm, n_slots, 0);
    if (get_prim_type(reference) != TYPE_REFERENCE) {
        run_gc(vm);
        reference = g_malloc_direct(thread->vm, n_slots, 1);
    }
    return reference;
}

void g_free(VM_Thread *thread, prim_val reference) {
    VM *vm = thread->vm;

    if (get_prim_type(reference) != TYPE_REFERENCE) {
        vm_error("HEAP", "g_free called on a non-reference value!");
        return;
    }
    word data_addr = reference.data;
    if (data_addr == 0 || data_addr - 1 >= VM_HEAP_SLOTS) {
        vm_error("HEAP", "Invalid reference address 0x%X!", data_addr);
        return;
    }

    g_free_direct(thread->vm, data_addr);

}

void g_free_direct(VM *vm, word data_addr){
    word header_addr = data_addr - 1;
    prim_val header = vm->ram[header_addr];
    if (get_prim_type(header) != TYPE_LENGTH) {
        vm_error("HEAP", "Double free or corrupted reference at 0x%X!", data_addr);
        return;
    }
    word block_size = header.data;

    /* Invalidate the header immediately so a repeat g_free on the same
     * reference is caught above as corruption, rather than silently
     * freeing memory that now belongs to someone else. */
    vm->ram[header_addr] = make_prim_val(0, STATE_OPEN, TYPE_NULL);

    if (block_size <= 32) {
        slab_free(vm, header_addr);
    } else {
        byte order = 0;
        while (((word)VM_BUDDY_BASE_BLOCK_SLOTS << order) < block_size) order++;
        buddy_free(&vm->allocator.buddy, header_addr, order);
    }
}
