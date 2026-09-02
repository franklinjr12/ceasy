#ifndef CEASY_MEMORY_ALLOCATOR_H
#define CEASY_MEMORY_ALLOCATOR_H

#include <stddef.h>

#include <ceasy/memory/arena.h>

typedef void *(*AllocatorAllocFn)(void *context, size_t size);
typedef void *(*AllocatorReallocFn)(void *context, void *memory,
                                    size_t old_size, size_t new_size);
typedef void (*AllocatorFreeFn)(void *context, void *memory);

typedef struct {
    void *context;
    AllocatorAllocFn alloc;
    AllocatorReallocFn realloc;
    AllocatorFreeFn free;
} Allocator;

/* Returns an allocator whose storage is released with string_destroy. */
Allocator allocator_heap(void);
Allocator heap_allocator(void);

/* Returns an allocator whose storage is released with arena_destroy. */
Allocator arena_allocator(Arena *arena);

#endif
