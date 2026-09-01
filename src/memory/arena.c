#include "ceasy/memory/arena.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

struct ArenaBlock {
    ArenaBlock *next;
    size_t capacity;
    size_t used;
    size_t allocation_count;
    bool oversized;
    max_align_t alignment;
    unsigned char data[];
};

static size_t arena_alignment(void) { return _Alignof(max_align_t); }

static bool arena_align_up(size_t value, size_t *aligned) {
    size_t alignment = arena_alignment();
    size_t remainder = value % alignment;
    size_t padding;

    if (remainder == 0) {
        *aligned = value;
        return true;
    }

    padding = alignment - remainder;
    if (value > SIZE_MAX - padding) {
        return false;
    }

    *aligned = value + padding;
    return true;
}

static bool arena_required_capacity(size_t size, size_t *capacity) {
    size_t alignment_padding = arena_alignment() - 1;

    if (size > SIZE_MAX - alignment_padding) {
        return false;
    }

    *capacity = size + alignment_padding;
    return true;
}

static bool arena_capacity_is_oversized(const Arena *arena, size_t capacity) {
    size_t threshold;

    if (arena->default_block_size > SIZE_MAX / 4) {
        return false;
    }

    threshold = arena->default_block_size * 4;
    return capacity > threshold;
}

static ArenaBlock *arena_block_create(const Arena *arena, size_t capacity) {
    ArenaBlock *block;

    if (capacity > SIZE_MAX - sizeof(*block)) {
        return NULL;
    }

    block = malloc(sizeof(*block) + capacity);
    if (block == NULL) {
        return NULL;
    }

    block->next = NULL;
    block->capacity = capacity;
    block->used = 0;
    block->allocation_count = 0;
    block->oversized = arena_capacity_is_oversized(arena, capacity);
    return block;
}

static void arena_block_free_chain(ArenaBlock *block) {
    while (block != NULL) {
        ArenaBlock *next = block->next;

        free(block);
        block = next;
    }
}

static bool arena_block_can_allocate(const ArenaBlock *block, size_t size) {
    size_t aligned_used;

    if (!arena_align_up(block->used, &aligned_used) ||
        aligned_used > block->capacity ||
        size > block->capacity - aligned_used) {
        return false;
    }

    return true;
}

static bool arena_add_block(Arena *arena, size_t size) {
    ArenaBlock *block;
    size_t required_capacity;
    size_t capacity;

    if (!arena_required_capacity(size, &required_capacity)) {
        return false;
    }

    capacity = arena->default_block_size;
    if (capacity < required_capacity) {
        capacity = required_capacity;
    }

    if (arena->block_count == SIZE_MAX ||
        arena->capacity > SIZE_MAX - capacity) {
        return false;
    }

    block = arena_block_create(arena, capacity);
    if (block == NULL) {
        return false;
    }

    if (arena->current == NULL) {
        arena->first = block;
    } else {
        block->next = arena->current->next;
        arena->current->next = block;
    }

    arena->current = block;
    arena->block_count++;
    arena->capacity += capacity;
    return true;
}

bool arena_init(Arena *arena, size_t block_size) {
    if (arena == NULL || block_size == 0) {
        return false;
    }

    memset(arena, 0, sizeof(*arena));
    arena->default_block_size = block_size;
    arena->generation = 1;
    return true;
}

void *arena_alloc(Arena *arena, size_t size) {
    ArenaBlock *block;
    size_t aligned_used;
    size_t previous_used;

    if (arena == NULL || arena->default_block_size == 0 || size == 0) {
        return NULL;
    }

    block = arena->current;
    if (block == NULL || !arena_block_can_allocate(block, size)) {
        if (block != NULL && block->next != NULL &&
            arena_block_can_allocate(block->next, size)) {
            arena->current = block->next;
        } else if (!arena_add_block(arena, size)) {
            return NULL;
        }
        block = arena->current;
    }

    previous_used = block->used;
    if (!arena_align_up(previous_used, &aligned_used) ||
        size > block->capacity - aligned_used) {
        return NULL;
    }

    block->used = aligned_used + size;
    block->allocation_count++;
    arena->used += block->used - previous_used;
    arena->allocation_count++;
    return block->data + aligned_used;
}

void *arena_alloc_zero(Arena *arena, size_t size) {
    void *memory = arena_alloc(arena, size);

    if (memory != NULL) {
        memset(memory, 0, size);
    }

    return memory;
}

void *arena_alloc_array(Arena *arena, size_t count, size_t element_size) {
    if (count != 0 && element_size > SIZE_MAX / count) {
        return NULL;
    }

    return arena_alloc(arena, count * element_size);
}

ArenaMark arena_mark(Arena *arena) {
    ArenaMark mark;

    memset(&mark, 0, sizeof(mark));
    if (arena == NULL || arena->default_block_size == 0) {
        return mark;
    }

    mark.arena = arena;
    mark.block = arena->current;
    mark.used = arena->current == NULL ? 0 : arena->current->used;
    mark.allocation_count =
        arena->current == NULL ? 0 : arena->current->allocation_count;
    mark.total_used = arena->used;
    mark.total_allocations = arena->allocation_count;
    mark.generation = arena->generation;
    return mark;
}

static bool arena_contains_block(const Arena *arena, const ArenaBlock *target) {
    const ArenaBlock *block = arena->first;

    while (block != NULL) {
        if (block == target) {
            return true;
        }
        block = block->next;
    }

    return false;
}

static void arena_reset_to_empty(Arena *arena) {
    arena_block_free_chain(arena->first);
    arena->first = NULL;
    arena->current = NULL;
    arena->block_count = 0;
    arena->capacity = 0;
    arena->used = 0;
    arena->allocation_count = 0;
}

bool arena_restore(Arena *arena, ArenaMark mark) {
    ArenaBlock *released;

    if (arena == NULL || mark.arena != arena ||
        mark.generation != arena->generation) {
        return false;
    }

    if (mark.block == NULL) {
        if (mark.total_used != 0 || mark.total_allocations != 0) {
            return false;
        }
        arena_reset_to_empty(arena);
        return true;
    }

    if (!arena_contains_block(arena, mark.block) ||
        mark.used > mark.block->used ||
        mark.allocation_count > mark.block->allocation_count) {
        return false;
    }

    released = mark.block->next;
    mark.block->next = NULL;
    arena_block_free_chain(released);
    arena->current = mark.block;
    mark.block->used = mark.used;
    mark.block->allocation_count = mark.allocation_count;
    arena->used = mark.total_used;
    arena->allocation_count = mark.total_allocations;

    arena->block_count = 0;
    arena->capacity = 0;
    for (ArenaBlock *block = arena->first; block != NULL; block = block->next) {
        arena->block_count++;
        arena->capacity += block->capacity;
    }
    return true;
}

void arena_reset(Arena *arena) {
    ArenaBlock *block;
    ArenaBlock *retained_first = NULL;
    ArenaBlock *retained_last = NULL;

    if (arena == NULL || arena->default_block_size == 0) {
        return;
    }

    block = arena->first;
    while (block != NULL) {
        ArenaBlock *next = block->next;

        if (block->oversized) {
            size_t capacity = block->capacity;

            free(block);
            arena->block_count--;
            arena->capacity -= capacity;
        } else {
            block->next = NULL;
            block->used = 0;
            block->allocation_count = 0;
            if (retained_last == NULL) {
                retained_first = block;
            } else {
                retained_last->next = block;
            }
            retained_last = block;
        }

        block = next;
    }

    arena->first = retained_first;
    arena->current = retained_first;
    arena->used = 0;
    arena->allocation_count = 0;
    arena->generation++;
    if (arena->generation == 0) {
        arena->generation = 1;
    }
}

void arena_destroy(Arena *arena) {
    if (arena == NULL) {
        return;
    }

    arena_block_free_chain(arena->first);
    memset(arena, 0, sizeof(*arena));
}

ArenaStats arena_stats(const Arena *arena) {
    ArenaStats stats;

    memset(&stats, 0, sizeof(stats));
    if (arena == NULL) {
        return stats;
    }

    stats.block_count = arena->block_count;
    stats.capacity = arena->capacity;
    stats.used = arena->used;
    stats.allocation_count = arena->allocation_count;
    return stats;
}
