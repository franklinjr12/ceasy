#include "ceasy/memory/arena.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    int id;
    double score;
} TestUser;

static void assert_aligned(const void *memory, size_t alignment) {
    assert(memory != NULL);
    assert((uintptr_t)memory % alignment == 0);
}

static void test_initialize_and_empty_destroy(void) {
    Arena arena;
    ArenaStats stats;

    assert(arena_init(&arena, 64));
    stats = arena_stats(&arena);
    assert(stats.block_count == 0);
    assert(stats.capacity == 0);
    assert(stats.used == 0);
    assert(stats.allocation_count == 0);
    arena_destroy(&arena);
    arena_destroy(&arena);
    assert(arena_stats(&arena).block_count == 0);
    assert(!arena_init(&arena, 0));
    assert(arena_alloc(&arena, 1) == NULL);
}

static void test_basic_and_typed_allocations(void) {
    Arena arena;
    int *number;
    TestUser *user;
    TestUser *users;

    assert(arena_init(&arena, 256));
    assert(arena_alloc(&arena, 0) == NULL);

    number = arena_new(&arena, int);
    user = arena_new_zero(&arena, TestUser);
    users = arena_new_array(&arena, TestUser, 4);
    assert(number != NULL);
    assert(user != NULL);
    assert(users != NULL);
    assert(number != (int *)user);
    assert(number != (int *)users);

    *number = 42;
    user->id = 7;
    user->score = 3.5;
    for (size_t index = 0; index < 4; index++) {
        users[index].id = (int)index;
        users[index].score = (double)index * 2.0;
    }

    assert(*number == 42);
    assert(user->id == 7);
    assert(user->score == 3.5);
    for (size_t index = 0; index < 4; index++) {
        assert(users[index].id == (int)index);
        assert(users[index].score == (double)index * 2.0);
    }
    assert(arena_stats(&arena).allocation_count == 3);
    arena_destroy(&arena);
}

static void test_zeroed_and_array_overflow(void) {
    Arena arena;
    unsigned char *bytes;
    ArenaStats before;

    assert(arena_init(&arena, 64));
    bytes = arena_alloc_zero(&arena, 257);
    assert(bytes != NULL);
    for (size_t index = 0; index < 257; index++) {
        assert(bytes[index] == 0);
    }

    before = arena_stats(&arena);
    assert(arena_alloc(&arena, SIZE_MAX) == NULL);
    assert(arena_alloc_array(&arena, SIZE_MAX, 2) == NULL);
    assert(arena_alloc_array(&arena, 0, SIZE_MAX) == NULL);
    assert(arena_stats(&arena).allocation_count == before.allocation_count);
    assert(arena_stats(&arena).used == before.used);
    arena_destroy(&arena);
}

static void test_alignment(void) {
    Arena arena;

    assert(arena_init(&arena, 256));
    assert_aligned(arena_alloc(&arena, sizeof(char)), _Alignof(char));
    assert_aligned(arena_alloc(&arena, sizeof(int)), _Alignof(int));
    assert_aligned(arena_alloc(&arena, sizeof(double)), _Alignof(double));
    assert_aligned(arena_alloc(&arena, sizeof(long double)),
                   _Alignof(long double));
    assert_aligned(arena_alloc(&arena, sizeof(max_align_t)),
                   _Alignof(max_align_t));
    arena_destroy(&arena);
}

static void test_growth_and_pointer_stability(void) {
    Arena arena;
    int *first;
    ArenaStats stats;

    assert(arena_init(&arena, 32));
    first = arena_new(&arena, int);
    assert(first != NULL);
    *first = 123456;

    for (size_t index = 0; index < 100; index++) {
        unsigned char *memory = arena_alloc(&arena, 40);

        assert(memory != NULL);
        memory[0] = (unsigned char)index;
    }

    stats = arena_stats(&arena);
    assert(stats.block_count > 1);
    assert(stats.allocation_count == 101);
    assert(*first == 123456);
    arena_destroy(&arena);
}

static void test_large_allocation(void) {
    Arena arena;
    unsigned char *prefix;
    unsigned char *large;
    ArenaStats stats;

    assert(arena_init(&arena, 64));
    prefix = arena_alloc(&arena, 8);
    large = arena_alloc(&arena, 4096);
    assert(prefix != NULL);
    assert(large != NULL);
    prefix[0] = 0x5a;
    large[0] = 0xa5;
    large[4095] = 0x3c;
    stats = arena_stats(&arena);
    assert(stats.block_count == 2);
    assert(stats.capacity >= 4096);
    assert(prefix[0] == 0x5a);
    assert(large[0] == 0xa5);
    assert(large[4095] == 0x3c);
    arena_destroy(&arena);
}

static void test_many_allocations(void) {
    Arena arena;
    uintptr_t addresses[2048];

    assert(arena_init(&arena, 64));
    for (size_t index = 0; index < 2048; index++) {
        unsigned int *value = arena_new(&arena, unsigned int);

        assert(value != NULL);
        *value = (unsigned int)index;
        addresses[index] = (uintptr_t)value;
        for (size_t previous = 0; previous < index; previous++) {
            assert(addresses[previous] != addresses[index]);
        }
    }
    assert(arena_stats(&arena).allocation_count == 2048);
    arena_destroy(&arena);
}

static void test_reset_and_reuse(void) {
    Arena arena;
    ArenaStats before;
    ArenaStats after;
    int *value;

    assert(arena_init(&arena, 64));
    value = arena_new(&arena, int);
    assert(value != NULL);
    *value = 9;
    arena_alloc(&arena, 100);
    before = arena_stats(&arena);
    arena_reset(&arena);
    after = arena_stats(&arena);
    assert(after.block_count == before.block_count);
    assert(after.capacity == before.capacity);
    assert(after.used == 0);
    assert(after.allocation_count == 0);

    value = arena_new(&arena, int);
    assert(value != NULL);
    *value = 10;
    assert(*value == 10);
    assert(arena_stats(&arena).allocation_count == 1);
    arena_reset(&arena);
    arena_reset(&arena);
    assert(arena_stats(&arena).used == 0);
    arena_destroy(&arena);
}

static void test_reset_releases_oversized_blocks(void) {
    Arena arena;
    ArenaStats before;

    assert(arena_init(&arena, 64));
    assert(arena_alloc(&arena, 4096) != NULL);
    before = arena_stats(&arena);
    assert(before.block_count == 1);
    arena_reset(&arena);
    assert(arena_stats(&arena).block_count == 0);
    assert(arena_stats(&arena).capacity == 0);
    assert(arena_alloc(&arena, 16) != NULL);
    arena_destroy(&arena);
}

static void test_mark_restore_same_block(void) {
    Arena arena;
    int *first;
    int *second;
    int *replacement;
    ArenaMark mark;

    assert(arena_init(&arena, 256));
    first = arena_new(&arena, int);
    assert(first != NULL);
    *first = 11;
    mark = arena_mark(&arena);
    second = arena_new(&arena, int);
    assert(second != NULL);
    assert(arena_restore(&arena, mark));
    replacement = arena_new(&arena, int);
    assert(replacement == second);
    assert(*first == 11);
    arena_destroy(&arena);
}

static void test_mark_restore_across_blocks(void) {
    Arena arena;
    int *first;
    ArenaMark mark;
    ArenaStats after_restore;

    assert(arena_init(&arena, 64));
    first = arena_new(&arena, int);
    assert(first != NULL);
    *first = 77;
    mark = arena_mark(&arena);
    assert(arena_alloc(&arena, 256) != NULL);
    assert(arena_alloc(&arena, 512) != NULL);
    assert(arena_stats(&arena).block_count > 1);
    assert(arena_restore(&arena, mark));
    after_restore = arena_stats(&arena);
    assert(after_restore.block_count == 1);
    assert(after_restore.used == sizeof(int));
    assert(after_restore.allocation_count == 1);
    assert(*first == 77);
    arena_destroy(&arena);
}

static void test_nested_and_invalid_marks(void) {
    Arena arena;
    Arena other;
    ArenaMark mark_a;
    ArenaMark mark_b;

    assert(arena_init(&arena, 256));
    assert(arena_init(&other, 256));
    mark_a = arena_mark(&arena);
    assert(arena_new(&arena, int) != NULL);
    mark_b = arena_mark(&arena);
    assert(arena_new(&arena, int) != NULL);
    assert(arena_restore(&arena, mark_b));
    assert(arena_new(&arena, int) != NULL);
    assert(arena_restore(&arena, mark_a));
    assert(arena_stats(&arena).allocation_count == 0);
    assert(!arena_restore(&other, mark_a));

    mark_a = arena_mark(&arena);
    assert(arena_new(&arena, int) != NULL);
    arena_reset(&arena);
    assert(!arena_restore(&arena, mark_a));

    mark_a = arena_mark(&arena);
    arena_destroy(&arena);
    assert(!arena_restore(&arena, mark_a));
    arena_destroy(&other);
}

static void test_statistics_and_destroy(void) {
    Arena arena;
    ArenaMark mark;
    ArenaStats stats;

    assert(arena_init(&arena, 64));
    assert(arena_stats(&arena).block_count == 0);
    assert(arena_alloc(&arena, 8) != NULL);
    mark = arena_mark(&arena);
    assert(arena_alloc(&arena, 200) != NULL);
    stats = arena_stats(&arena);
    assert(stats.block_count == 2);
    assert(stats.capacity >= 64 + 200);
    assert(stats.used >= 208);
    assert(stats.allocation_count == 2);
    assert(arena_restore(&arena, mark));
    stats = arena_stats(&arena);
    assert(stats.block_count == 1);
    assert(stats.used == 8);
    assert(stats.allocation_count == 1);
    arena_reset(&arena);
    stats = arena_stats(&arena);
    assert(stats.used == 0);
    assert(stats.allocation_count == 0);
    arena_destroy(&arena);
    stats = arena_stats(&arena);
    assert(stats.block_count == 0);
    assert(stats.capacity == 0);
    assert(stats.used == 0);
    assert(stats.allocation_count == 0);
}

int main(void) {
    test_initialize_and_empty_destroy();
    test_basic_and_typed_allocations();
    test_zeroed_and_array_overflow();
    test_alignment();
    test_growth_and_pointer_stability();
    test_large_allocation();
    test_many_allocations();
    test_reset_and_reuse();
    test_reset_releases_oversized_blocks();
    test_mark_restore_same_block();
    test_mark_restore_across_blocks();
    test_nested_and_invalid_marks();
    test_statistics_and_destroy();
    return 0;
}
