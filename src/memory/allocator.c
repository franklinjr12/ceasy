#include "ceasy/memory/allocator.h"

#include <stdlib.h>
#include <string.h>

static void *heap_alloc(void *context, size_t size) {
    (void)context;
    return malloc(size);
}

static void *heap_realloc(void *context, void *memory, size_t old_size,
                          size_t new_size) {
    (void)context;
    (void)old_size;
    return realloc(memory, new_size);
}

static void heap_free(void *context, void *memory) {
    (void)context;
    free(memory);
}

Allocator allocator_heap(void) {
    Allocator allocator;

    allocator.context = NULL;
    allocator.alloc = heap_alloc;
    allocator.realloc = heap_realloc;
    allocator.free = heap_free;
    return allocator;
}

Allocator heap_allocator(void) { return allocator_heap(); }

static void *arena_realloc(void *context, void *memory, size_t old_size,
                           size_t new_size) {
    Arena *arena = context;
    void *replacement;

    replacement = arena_alloc(arena, new_size);
    if (replacement == NULL) {
        return NULL;
    }
    if (memory != NULL && old_size != 0) {
        memcpy(replacement, memory, old_size < new_size ? old_size : new_size);
    }
    return replacement;
}

static void *arena_allocator_alloc(void *context, size_t size) {
    return arena_alloc(context, size);
}

static void arena_allocator_free(void *context, void *memory) {
    (void)context;
    (void)memory;
}

Allocator arena_allocator(Arena *arena) {
    Allocator allocator;

    allocator.context = arena;
    allocator.alloc = arena_allocator_alloc;
    allocator.realloc = arena_realloc;
    allocator.free = arena_allocator_free;
    return allocator;
}
