#include <ceasy/ceasy.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int64_t id;
    bool enabled;
} TestObject;

typedef struct {
    size_t allocations;
    size_t fail_after;
} FailureAllocator;

static void *failure_alloc(void *context, size_t size) {
    FailureAllocator *failure = context;

    if (failure->allocations >= failure->fail_after) {
        return NULL;
    }
    failure->allocations++;
    return malloc(size);
}

static void *failure_realloc(void *context, void *memory, size_t old_size,
                             size_t new_size) {
    FailureAllocator *failure = context;

    (void)old_size;
    if (failure->allocations >= failure->fail_after) {
        return NULL;
    }
    failure->allocations++;
    return realloc(memory, new_size);
}

static void failure_free(void *context, void *memory) {
    (void)context;
    free(memory);
}

static void test_mixed_values_and_getters(void) {
    StringMap map = {0};
    TestObject input = {.id = 42, .enabled = true};
    TestObject *stored_object;
    StringView stored_string;
    void *stored_pointer;
    int64_t integer;
    bool boolean;
    int service = 7;

    assert(sm_init_heap(&map));
    assert(sm_set(&map, sv("number"), 7));
    assert(sm_set(&map, sv("title"), sv("Hello")));
    assert(sm_set(&map, sv("active"), true));
    assert(sm_set(&map, sv("service"), sm_pointer(&service)));
    assert(sm_set(&map, sv("object"), sm_struct(&map, input)));

    assert(sm_get_int(&map, sv("number"), &integer) && integer == 7);
    assert(sm_get_string(&map, sv("title"), &stored_string));
    assert(stringv_equal(stored_string, sv("Hello")));
    assert(sm_get_bool(&map, sv("active"), &boolean) && boolean);
    assert(sm_get_pointer(&map, sv("service"), &stored_pointer) &&
           stored_pointer == &service);
    stored_object = sm_get_object(&map, sv("object"), sizeof(*stored_object));
    assert(stored_object != NULL && stored_object != &input);
    assert(stored_object->id == 42 && stored_object->enabled);
    assert(sm_get_object(&map, sv("object"), sizeof(int)) == NULL);
    assert(!sm_get_bool(&map, sv("number"), &boolean));
    assert(sm_set(&map, sv("replace"), sm_int(1)));
    assert(sm_set(&map, sv("replace"), sv("temporary")));
    assert(sm_set(&map, sv("replace"), sm_struct(&map, input)));
    stored_object = sm_get_object(&map, sv("replace"), sizeof(*stored_object));
    assert(stored_object != NULL && stored_object->id == 42);
    assert(sm_length(&map) == 6);
    sm_destroy(&map);
}

static void test_key_and_value_copying(void) {
    StringMap map = {0};
    String key = string_from_heap(sv("dynamic-key"));
    String string = string_from_heap(sv("dynamic-value"));
    StringView embedded_key = {.data = "a\0b", .length = 3};
    StringView embedded_value = {.data = "x\0y", .length = 3};
    StringView result;

    assert(sm_init_heap(&map));
    assert(sm_set(&map, string_as_view(&key), string_as_view(&string)));
    string_destroy(&key);
    string_destroy(&string);
    assert(sm_get_string(&map, sv("dynamic-key"), &result));
    assert(stringv_equal(result, sv("dynamic-value")));

    assert(sm_set(&map, embedded_key, embedded_value));
    assert(sm_get_string(&map, embedded_key, &result));
    assert(result.length == 3 && memcmp(result.data, "x\0y", 3) == 0);
    assert(sm_set_value_borrowed_key(&map, sv("borrowed"),
                                     sm_string_borrowed(sv("literal"))));
    assert(sm_get_string(&map, sv("borrowed"), &result));
    assert(stringv_equal(result, sv("literal")));
    assert(sm_set(&map, (StringView){0}, sv("empty-key")));
    assert(sm_set(&map, sv("empty-value"), (StringView){0}));
    assert(sm_contains(&map, (StringView){0}));
    assert(sm_get_string(&map, sv("empty-value"), &result) &&
           result.length == 0);
    sm_destroy(&map);
}

static void test_replacement_removal_resize_and_iteration(void) {
    StringMap map = {0};
    StringMapIterator iterator;
    StringMapValue *value;
    StringView key;
    size_t visited = 0;

    assert(sm_init_heap(&map));
    for (int index = 0; index < 100; index++) {
        char key_data[32];
        int length = snprintf(key_data, sizeof(key_data), "key-%d", index);

        assert(length > 0);
        assert(sm_set(
            &map, ((StringView){.data = key_data, .length = (size_t)length}),
            sm_int(index)));
    }
    assert(map.capacity >= 128);
    for (int index = 0; index < 100; index++) {
        char key_data[32];
        int64_t result;
        int length = snprintf(key_data, sizeof(key_data), "key-%d", index);

        assert(sm_get_int(
            &map, ((StringView){.data = key_data, .length = (size_t)length}),
            &result));
        assert(result == index);
    }
    assert(sm_set(&map, sv("key-50"), sv("replaced")));
    assert(sm_length(&map) == 100);
    assert(sm_remove(&map, sv("key-50")));
    assert(!sm_contains(&map, sv("key-50")));
    assert(!sm_remove(&map, sv("key-50")));
    assert(sm_set(&map, sv("replacement"), 123));
    assert(sm_length(&map) == 100);

    iterator = sm_iterator();
    while (sm_next(&map, &iterator, &key, &value)) {
        assert(key.data != NULL || key.length == 0);
        assert(value != NULL);
        visited++;
    }
    assert(visited == sm_length(&map));
    sm_clear(&map);
    assert(sm_length(&map) == 0);
    assert(map.capacity >= 128);
    assert(sm_set(&map, sv("after-clear"), 1));
    sm_destroy(&map);
    sm_destroy(&map);
}

static void test_arena_map(void) {
    Arena arena;
    StringMap map = {0};
    TestObject object = {.id = 9, .enabled = false};

    assert(arena_init(&arena, 64));
    assert(sm_init_in(&map, &arena));
    for (int index = 0; index < 80; index++) {
        char key_data[32];
        int length =
            snprintf(key_data, sizeof(key_data), "arena-key-%d", index);

        assert(length > 0);
        assert(sm_set(
            &map, ((StringView){.data = key_data, .length = (size_t)length}),
            sm_int(index)));
    }
    assert(sm_set(&map, sv("object"), sm_struct(&map, object)));
    object.id = 100;
    assert(
        ((TestObject *)sm_get_object(&map, sv("object"), sizeof(object)))->id ==
        9);
    sm_destroy(&map);
    arena_destroy(&arena);
}

static void test_failed_resize_preserves_map(void) {
    FailureAllocator failure = {.fail_after = SIZE_MAX};
    Allocator allocator = {.context = &failure,
                           .alloc = failure_alloc,
                           .realloc = failure_realloc,
                           .free = failure_free};
    StringMap map = {0};
    int64_t result;

    assert(sm_init(&map, allocator));
    for (int index = 0; index < 10; index++) {
        char key[16];
        int length = snprintf(key, sizeof(key), "key-%d", index);

        assert(sm_set(&map,
                      ((StringView){.data = key, .length = (size_t)length}),
                      sm_int(index)));
    }
    failure.fail_after = failure.allocations + 1;
    assert(!sm_set(&map, sv("blocked"), 999));
    assert(sm_length(&map) == 10);
    assert(!sm_contains(&map, sv("blocked")));
    assert(sm_get_int(&map, sv("key-9"), &result) && result == 9);
    failure.fail_after = SIZE_MAX;
    assert(sm_set(&map, sv("blocked"), sm_int(999)));
    sm_destroy(&map);
}

int main(void) {
    test_mixed_values_and_getters();
    test_key_and_value_copying();
    test_replacement_removal_resize_and_iteration();
    test_arena_map();
    test_failed_resize_preserves_map();
    return 0;
}
