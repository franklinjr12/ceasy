#ifndef CEASY_MEMORY_ARENA_H
#define CEASY_MEMORY_ARENA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ArenaBlock ArenaBlock;

typedef struct Arena {
    ArenaBlock *first;
    ArenaBlock *current;
    size_t default_block_size;
    size_t block_count;
    size_t capacity;
    size_t used;
    size_t allocation_count;
    uint64_t generation;
} Arena;

typedef struct {
    const Arena *arena;
    ArenaBlock *block;
    size_t used;
    size_t allocation_count;
    size_t total_used;
    size_t total_allocations;
    uint64_t generation;
} ArenaMark;

typedef struct {
    size_t block_count;
    size_t capacity;
    size_t used;
    size_t allocation_count;
} ArenaStats;

/* Initializes lazy arena. First heap block appears on first allocation. */
bool arena_init(Arena *arena, size_t block_size);

/* Returns aligned arena-owned memory, or NULL on zero size or failure. */
void *arena_alloc(Arena *arena, size_t size);

/* Like arena_alloc, but clears requested bytes before returning. */
void *arena_alloc_zero(Arena *arena, size_t size);

/* Allocates count * element_size bytes with overflow protection. */
void *arena_alloc_array(Arena *arena, size_t count, size_t element_size);

#define arena_new(arena, Type) ((Type *)arena_alloc((arena), sizeof(Type)))
#define arena_new_zero(arena, Type)                                            \
    ((Type *)arena_alloc_zero((arena), sizeof(Type)))
#define arena_new_array(arena, Type, count)                                    \
    ((Type *)arena_alloc_array((arena), (count), sizeof(Type)))

/* Marks current position. Restore frees every block after marked block. */
ArenaMark arena_mark(Arena *arena);

/* Discards allocations after mark. Returns false for detectable invalid marks.
 */
bool arena_restore(Arena *arena, ArenaMark mark);

/* Invalidates all allocations; retains normal blocks, frees blocks over 4x
 * default size. */
void arena_reset(Arena *arena);

/* Releases all blocks and leaves arena empty and uninitialized. */
void arena_destroy(Arena *arena);

/* Reports live blocks, reserved bytes, bump-used bytes (including alignment
 * padding), and post-reset allocations. */
ArenaStats arena_stats(const Arena *arena);

/*
 * Individual allocations are never freed. All allocations are released
 * together via
 * arena_restore, arena_reset, or arena_destroy. Growth never moves old data.
 * Pointers become invalid after a reset/restore covering them or destruction.
 * Arena instances are not thread-safe and need external synchronization.
 */

#endif
