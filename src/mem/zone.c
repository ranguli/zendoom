//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	Zone Memory Allocation. Neat.
//

#include <string.h>

#include "../impl/system.h"
#include "../lib/argv.h"
#include "../lib/type.h"

#include "zone.h"

//
// ZONE MEMORY ALLOCATION
//
// There is never any space between memblocks,
//  and there will never be two contiguous free memblocks.
// The rover can be left pointing at a non-empty block.
//
// It is of no value to free a cachable block,
//  because it will get overwritten automatically if needed.
//

#define MEM_ALIGN sizeof(void *)
#define ZONEID 0x1d4a11

typedef struct memblock_s {
    int size; // including the header and possibly tiny fragments
    void **user;
    int tag; // PU_FREE if this is free
    int id;  // should be ZONEID
    struct memblock_s *next;
    struct memblock_s *prev;
} memblock_t;

typedef struct {
    // total bytes malloced, including header
    int size;

    // start / end cap for linked list
    memblock_t blocklist;

    memblock_t *rover;

} memzone_t;

static memzone_t *mainzone;
static boolean zero_on_free;
static boolean scan_on_free;

//
// Z_Init
//
void Z_Init(void) {
    memblock_t *block;
    int size;

    mainzone = (memzone_t *)I_ZoneBase(&size);
    mainzone->size = size;

    // set the entire zone to one free block
    mainzone->blocklist.next = mainzone->blocklist.prev = block =
        (memblock_t *)((byte *)mainzone + sizeof(memzone_t));

    mainzone->blocklist.user = (void *)mainzone;
    mainzone->blocklist.tag = PU_STATIC;
    mainzone->rover = block;

    block->prev = block->next = &mainzone->blocklist;

    // free block
    block->tag = PU_FREE;

    block->size = mainzone->size - sizeof(memzone_t);

    // [Deliberately undocumented]
    // Zone memory debugging flag. If set, memory is zeroed after it is freed
    // to deliberately break any code that attempts to use it after free.
    //
    zero_on_free = M_ParmExists("-zonezero");

    // [Deliberately undocumented]
    // Zone memory debugging flag. If set, each time memory is freed, the zone
    // heap is scanned to look for remaining pointers to the freed block.
    //
    scan_on_free = M_ParmExists("-zonescan");
}

// Scan the zone heap for pointers within the specified range, and warn about
// any remaining pointers.
static void ScanForBlock(void *start, void *end) {
    memblock_t *block;
    void **mem;
    int i, len;

    block = mainzone->blocklist.next;

    while (block->next != &mainzone->blocklist) {
        int tag = block->tag;

        if (tag == PU_STATIC || tag == PU_LEVEL || tag == PU_LEVSPEC) {
            // Scan for pointers on the assumption that pointers are aligned
            // on word boundaries (word size depending on pointer size):
            mem = (void **)((byte *)block + sizeof(memblock_t));
            len = (block->size - sizeof(memblock_t)) / sizeof(void *);

            for (i = 0; i < len; ++i) {
                if (start <= mem[i] && mem[i] <= end) {
                    fprintf(stderr,
                            "%p has dangling pointer into freed block "
                            "%p (%p -> %p)\n",
                            mem, start, &mem[i], mem[i]);
                }
            }
        }

        block = block->next;
    }
}

//
// Z_Free
//
void Z_Free(void *ptr) {
    memblock_t *block;
    memblock_t *other;

    block = (memblock_t *)((byte *)ptr - sizeof(memblock_t));

    if (block->id != ZONEID)
        error("Z_Free: freed a pointer without ZONEID");

    if (block->tag != PU_FREE && block->user != NULL) {
        // clear the user's mark
        *block->user = 0;
    }

    // mark as free
    block->tag = PU_FREE;
    block->user = NULL;
    block->id = 0;

    // If the -zonezero flag is provided, we zero out the block on free
    // to break code that depends on reading freed memory.
    if (zero_on_free) {
        memset(ptr, 0, block->size - sizeof(memblock_t));
    }
    if (scan_on_free) {
        ScanForBlock(ptr, (byte *)ptr + block->size - sizeof(memblock_t));
    }

    other = block->prev;

    if (other->tag == PU_FREE) {
        // merge with previous free block
        other->size += block->size;
        other->next = block->next;
        other->next->prev = other;

        if (block == mainzone->rover)
            mainzone->rover = other;

        block = other;
    }

    other = block->next;
    if (other->tag == PU_FREE) {
        // merge the next free block onto the end
        block->size += other->size;
        block->next = other->next;
        block->next->prev = block;

        if (other == mainzone->rover)
            mainzone->rover = block;
    }
}

//
// Z_Malloc
// You can pass a NULL user if the tag is < PU_PURGELEVEL.
//
#define MINFRAGMENT 64

void *Z_Malloc(int size, int tag, void *user) {
    int extra;
    memblock_t *start;
    memblock_t *rover;
    memblock_t *newblock;
    memblock_t *base;
    void *result;

    size = (size + MEM_ALIGN - 1) & ~(MEM_ALIGN - 1);

    // scan through the block list,
    // looking for the first free block
    // of sufficient size,
    // throwing out any purgable blocks along the way.

    // account for size of block header
    size += sizeof(memblock_t);

    // if there is a free block behind the rover,
    //  back up over them
    base = mainzone->rover;

    if (base->prev->tag == PU_FREE)
        base = base->prev;

    rover = base;
    start = base->prev;

    do {
        if (rover == start) {
            // scanned all the way around the list
            error("Z_Malloc: failed on allocation of %i bytes", size);
        }

        if (rover->tag != PU_FREE) {
            if (rover->tag < PU_PURGELEVEL) {
                // hit a block that can't be purged,
                // so move base past it
                base = rover = rover->next;
            } else {
                // free the rover block (adding the size to base)

                // the rover can be the base block
                base = base->prev;
                Z_Free((byte *)rover + sizeof(memblock_t));
                base = base->next;
                rover = base->next;
            }
        } else {
            rover = rover->next;
        }

    } while (base->tag != PU_FREE || base->size < size);

    // found a block big enough
    extra = base->size - size;

    if (extra > MINFRAGMENT) {
        // there will be a free fragment after the allocated block
        newblock = (memblock_t *)((byte *)base + size);
        newblock->size = extra;

        newblock->tag = PU_FREE;
        newblock->user = NULL;
        newblock->prev = base;
        newblock->next = base->next;
        newblock->next->prev = newblock;

        base->next = newblock;
        base->size = size;
    }

    if (user == NULL && tag >= PU_PURGELEVEL)
        error("Z_Malloc: an owner is required for purgable blocks");

    base->user = user;
    base->tag = tag;

    result = (void *)((byte *)base + sizeof(memblock_t));

    if (base->user) {
        *base->user = result;
    }

    // next allocation will start looking here
    mainzone->rover = base->next;

    base->id = ZONEID;

    return result;
}

//
// Z_FreeTags
//
void Z_FreeTags(int lowtag, int hightag) {
    memblock_t *block;
    memblock_t *next;

    for (block = mainzone->blocklist.next; block != &mainzone->blocklist; block = next) {
        // get link before freeing
        next = block->next;

        // free block?
        if (block->tag == PU_FREE)
            continue;

        if (block->tag >= lowtag && block->tag <= hightag)
            Z_Free((byte *)block + sizeof(memblock_t));
    }
}

//
// Z_CheckHeap
//
void Z_CheckHeap(void) {
    memblock_t *block;

    for (block = mainzone->blocklist.next;; block = block->next) {
        if (block->next == &mainzone->blocklist) {
            // all blocks have been hit
            break;
        }

        if ((byte *)block + block->size != (byte *)block->next)
            error("Z_CheckHeap: block size does not touch the next block\n");

        if (block->next->prev != block)
            error("Z_CheckHeap: next block doesn't have proper back link\n");

        if (block->tag == PU_FREE && block->next->tag == PU_FREE)
            error("Z_CheckHeap: two consecutive free blocks\n");
    }
}

//
// Z_ChangeTag
//
void Z_ChangeTag2(void *ptr, int tag, const char *file, int line) {
    memblock_t *block;

    block = (memblock_t *)((byte *)ptr - sizeof(memblock_t));

    if (block->id != ZONEID)
        error("%s:%i: Z_ChangeTag: block without a ZONEID!", file, line);

    if (tag >= PU_PURGELEVEL && block->user == NULL)
        error("%s:%i: Z_ChangeTag: an owner is required "
                "for purgable blocks",
                file, line);

    block->tag = tag;
}