#include "ceasy/collection/string_map.h"

#include <limits.h>
#include <string.h>

#define STRING_MAP_INITIAL_CAPACITY 16

static bool string_map_initialized(const StringMap *map) {
    return map != NULL && map->allocator.alloc != NULL &&
           map->allocator.free != NULL;
}

static bool string_map_key_valid(StringView key) {
    return key.length == 0 || key.data != NULL;
}

static bool string_map_slot_bytes(size_t capacity, size_t *bytes) {
    if (capacity == 0 || capacity > SIZE_MAX / sizeof(StringMapSlot)) {
        return false;
    }
    *bytes = capacity * sizeof(StringMapSlot);
    return true;
}

static void string_map_value_release(StringMap *map, StringMapValue *value) {
    if (map == NULL || !string_map_initialized(map) || value == NULL ||
        !value->owns_data) {
        return;
    }
    if (value->type == SM_VALUE_STRING) {
        map->allocator.free(map->allocator.context, (void *)value->string.data);
    } else if (value->type == SM_VALUE_OBJECT) {
        map->allocator.free(map->allocator.context, value->object.data);
    }
    value->owns_data = false;
}

static void string_map_slot_release(StringMap *map, StringMapSlot *slot) {
    if (slot == NULL) {
        return;
    }
    if (slot->owns_key) {
        map->allocator.free(map->allocator.context, (void *)slot->key.data);
    }
    string_map_value_release(map, &slot->value);
    memset(slot, 0, sizeof(*slot));
}

static size_t string_map_find_index(const StringMap *map, StringView key,
                                    uint64_t hash, size_t *first_tombstone) {
    size_t index;

    if (first_tombstone != NULL) {
        *first_tombstone = SIZE_MAX;
    }
    if (!string_map_initialized(map) || map->slots == NULL ||
        map->capacity == 0 || !string_map_key_valid(key)) {
        return SIZE_MAX;
    }

    index = (size_t)(hash & (uint64_t)(map->capacity - 1));
    for (size_t probes = 0; probes < map->capacity; probes++) {
        const StringMapSlot *slot = &map->slots[index];

        if (slot->state == STRING_MAP_SLOT_EMPTY) {
            return SIZE_MAX;
        }
        if (slot->state == STRING_MAP_SLOT_TOMBSTONE) {
            if (first_tombstone != NULL && *first_tombstone == SIZE_MAX) {
                *first_tombstone = index;
            }
        } else if (slot->hash == hash && stringv_equal(slot->key, key)) {
            return index;
        }
        index = (index + 1) & (map->capacity - 1);
    }
    return SIZE_MAX;
}

static bool string_map_copy_bytes(StringMap *map, const void *source,
                                  size_t size, void **result) {
    void *copy;

    if (size == 0) {
        *result = NULL;
        return true;
    }
    if (source == NULL) {
        return false;
    }
    copy = map->allocator.alloc(map->allocator.context, size);
    if (copy == NULL) {
        return false;
    }
    memcpy(copy, source, size);
    *result = copy;
    return true;
}

static bool string_map_copy_key(StringMap *map, StringView key,
                                StringView *copy) {
    void *data;

    if (!string_map_copy_bytes(map, key.data, key.length, &data)) {
        return false;
    }
    *copy = (StringView){.data = data, .length = key.length};
    return true;
}

static bool string_map_rehash(StringMap *map, size_t new_capacity) {
    StringMapSlot *new_slots;
    StringMapSlot *old_slots;
    size_t bytes;

    if (!string_map_initialized(map) ||
        !string_map_slot_bytes(new_capacity, &bytes)) {
        return false;
    }
    new_slots = map->allocator.alloc(map->allocator.context, bytes);
    if (new_slots == NULL) {
        return false;
    }
    memset(new_slots, 0, bytes);

    for (size_t index = 0; index < map->capacity; index++) {
        StringMapSlot *old_slot = &map->slots[index];
        size_t new_index;

        if (old_slot->state != STRING_MAP_SLOT_OCCUPIED) {
            continue;
        }
        new_index = (size_t)(old_slot->hash & (uint64_t)(new_capacity - 1));
        while (new_slots[new_index].state == STRING_MAP_SLOT_OCCUPIED) {
            new_index = (new_index + 1) & (new_capacity - 1);
        }
        new_slots[new_index] = *old_slot;
    }

    old_slots = map->slots;
    map->slots = new_slots;
    map->capacity = new_capacity;
    map->tombstone_count = 0;
    if (old_slots != NULL) {
        map->allocator.free(map->allocator.context, old_slots);
    }
    return true;
}

static bool string_map_load_reached(size_t capacity, size_t used) {
    size_t threshold = capacity / 2 + capacity / 5;

    return threshold == 0 || used >= threshold;
}

static bool string_map_prepare_capacity(StringMap *map) {
    size_t used;
    size_t next_capacity;

    if (map->slots == NULL) {
        return string_map_rehash(map, STRING_MAP_INITIAL_CAPACITY);
    }
    if (map->length > SIZE_MAX - map->tombstone_count ||
        map->length + map->tombstone_count == SIZE_MAX) {
        return false;
    }
    used = map->length + map->tombstone_count + 1;
    if (!string_map_load_reached(map->capacity, used)) {
        return true;
    }

    if (map->length < map->capacity / 2 && map->tombstone_count > 0) {
        return string_map_rehash(map, map->capacity);
    }
    if (map->capacity > SIZE_MAX / 2) {
        return false;
    }
    next_capacity = map->capacity * 2;
    return string_map_rehash(map, next_capacity);
}

static bool string_map_set_internal(StringMap *map, StringView key,
                                    StringMapValue value, bool copy_key) {
    uint64_t hash;
    size_t index;
    size_t first_tombstone;
    StringView stored_key = key;
    bool owns_key = false;

    if (!string_map_initialized(map) || !string_map_key_valid(key) ||
        value.type == SM_VALUE_EMPTY ||
        (value.type == SM_VALUE_STRING &&
         !string_map_key_valid(value.string)) ||
        (value.type == SM_VALUE_OBJECT &&
         (value.object.data == NULL || value.object.size == 0))) {
        string_map_value_release(map, &value);
        return false;
    }

    hash = stringv_hash(key);
    index = string_map_find_index(map, key, hash, &first_tombstone);
    if (index != SIZE_MAX) {
        StringMapSlot *slot = &map->slots[index];

        string_map_value_release(map, &slot->value);
        slot->value = value;
        return true;
    }

    if (copy_key) {
        if (!string_map_copy_key(map, key, &stored_key)) {
            string_map_value_release(map, &value);
            return false;
        }
        owns_key = stored_key.length > 0;
    }
    if (!string_map_prepare_capacity(map)) {
        if (owns_key) {
            map->allocator.free(map->allocator.context,
                                (void *)stored_key.data);
        }
        string_map_value_release(map, &value);
        return false;
    }

    index = string_map_find_index(map, key, hash, &first_tombstone);
    if (index != SIZE_MAX) {
        StringMapSlot *slot = &map->slots[index];

        if (owns_key) {
            map->allocator.free(map->allocator.context,
                                (void *)stored_key.data);
        }
        string_map_value_release(map, &slot->value);
        slot->value = value;
        return true;
    }
    if (first_tombstone != SIZE_MAX) {
        index = first_tombstone;
        map->tombstone_count--;
    } else {
        index = (size_t)(hash & (uint64_t)(map->capacity - 1));
        while (map->slots[index].state == STRING_MAP_SLOT_OCCUPIED) {
            index = (index + 1) & (map->capacity - 1);
        }
    }
    map->slots[index] = (StringMapSlot){.hash = hash,
                                        .key = stored_key,
                                        .value = value,
                                        .state = STRING_MAP_SLOT_OCCUPIED,
                                        .owns_key = owns_key};
    map->length++;
    return true;
}

bool sm_init(StringMap *map, Allocator allocator) {
    if (map == NULL || allocator.alloc == NULL || allocator.free == NULL) {
        return false;
    }
    if (string_map_initialized(map)) {
        return false;
    }
    memset(map, 0, sizeof(*map));
    map->allocator = allocator;
    return true;
}

bool sm_init_in(StringMap *map, Arena *arena) {
    if (arena == NULL) {
        return false;
    }
    return sm_init(map, arena_allocator(arena));
}

bool sm_init_heap(StringMap *map) { return sm_init(map, allocator_heap()); }

StringMapValue sm_int(int64_t value) {
    return (StringMapValue){
        .type = SM_VALUE_INT64, .valid = true, .integer = value};
}

StringMapValue sm_bool(bool value) {
    return (StringMapValue){
        .type = SM_VALUE_BOOL, .valid = true, .boolean = value};
}

StringMapValue sm_string_copy(StringMap *map, StringView value) {
    void *data;

    if (!string_map_initialized(map) || !string_map_key_valid(value) ||
        !string_map_copy_bytes(map, value.data, value.length, &data)) {
        return (StringMapValue){0};
    }
    return (StringMapValue){.type = SM_VALUE_STRING,
                            .valid = true,
                            .owns_data = true,
                            .string = {.data = data, .length = value.length}};
}

StringMapValue sm_string_borrowed(StringView value) {
    if (!string_map_key_valid(value)) {
        return (StringMapValue){0};
    }
    return (StringMapValue){
        .type = SM_VALUE_STRING, .valid = true, .string = value};
}

StringMapValue sm_pointer(void *pointer) {
    return (StringMapValue){
        .type = SM_VALUE_POINTER, .valid = true, .pointer = pointer};
}

StringMapValue sm_object(StringMap *map, const void *data, size_t size) {
    void *copy;

    if (!string_map_initialized(map) || size == 0 ||
        !string_map_copy_bytes(map, data, size, &copy)) {
        return (StringMapValue){0};
    }
    return (StringMapValue){.type = SM_VALUE_OBJECT,
                            .valid = true,
                            .owns_data = true,
                            .object = {.data = copy, .size = size}};
}

bool sm_set_value(StringMap *map, StringView key, StringMapValue value) {
    return string_map_set_internal(map, key, value, true);
}

bool sm_set_value_borrowed_key(StringMap *map, StringView key,
                               StringMapValue value) {
    return string_map_set_internal(map, key, value, false);
}

bool sm_set_int(StringMap *map, StringView key, int value) {
    /* In C, stdbool.h defines true and false as int constants. Treat the
     * two boolean literals as booleans while sm_int(0/1) remains available
     * for callers that need an explicitly integer value. */
    if (value == 0 || value == 1) {
        return sm_set_value(map, key, sm_bool(value != 0));
    }
    return sm_set_value(map, key, sm_int((int64_t)value));
}

bool sm_set_long(StringMap *map, StringView key, long value) {
    return sm_set_value(map, key, sm_int((int64_t)value));
}

bool sm_set_long_long(StringMap *map, StringView key, long long value) {
    return sm_set_value(map, key, sm_int((int64_t)value));
}

bool sm_set_bool(StringMap *map, StringView key, bool value) {
    return sm_set_value(map, key, sm_bool(value));
}

bool sm_set_string_view(StringMap *map, StringView key, StringView value) {
    return sm_set_value(map, key, sm_string_copy(map, value));
}

StringMapValue *sm_get(StringMap *map, StringView key) {
    uint64_t hash;
    size_t index;

    if (!string_map_key_valid(key) || map == NULL || map->slots == NULL) {
        return NULL;
    }
    hash = stringv_hash(key);
    index = string_map_find_index(map, key, hash, NULL);
    return index == SIZE_MAX ? NULL : &map->slots[index].value;
}

const StringMapValue *sm_get_const(const StringMap *map, StringView key) {
    uint64_t hash;
    size_t index;

    if (!string_map_key_valid(key) || map == NULL || map->slots == NULL) {
        return NULL;
    }
    hash = stringv_hash(key);
    index = string_map_find_index(map, key, hash, NULL);
    return index == SIZE_MAX ? NULL : &map->slots[index].value;
}

bool sm_get_int(const StringMap *map, StringView key, int64_t *result) {
    const StringMapValue *value = sm_get_const(map, key);

    if (result == NULL || value == NULL || value->type != SM_VALUE_INT64) {
        return false;
    }
    *result = value->integer;
    return true;
}

bool sm_get_bool(const StringMap *map, StringView key, bool *result) {
    const StringMapValue *value = sm_get_const(map, key);

    if (result == NULL || value == NULL || value->type != SM_VALUE_BOOL) {
        return false;
    }
    *result = value->boolean;
    return true;
}

bool sm_get_string(const StringMap *map, StringView key, StringView *result) {
    const StringMapValue *value = sm_get_const(map, key);

    if (result == NULL || value == NULL || value->type != SM_VALUE_STRING) {
        return false;
    }
    *result = value->string;
    return true;
}

bool sm_get_pointer(const StringMap *map, StringView key, void **result) {
    const StringMapValue *value = sm_get_const(map, key);

    if (result == NULL || value == NULL || value->type != SM_VALUE_POINTER) {
        return false;
    }
    *result = value->pointer;
    return true;
}

const void *sm_get_object_const(const StringMap *map, StringView key,
                                size_t expected_size) {
    const StringMapValue *value = sm_get_const(map, key);

    if (value == NULL || value->type != SM_VALUE_OBJECT ||
        value->object.size != expected_size) {
        return NULL;
    }
    return value->object.data;
}

void *sm_get_object(StringMap *map, StringView key, size_t expected_size) {
    return (void *)sm_get_object_const(map, key, expected_size);
}

bool sm_contains(const StringMap *map, StringView key) {
    return sm_get_const(map, key) != NULL;
}

bool sm_remove(StringMap *map, StringView key) {
    uint64_t hash;
    size_t index;

    if (map == NULL || map->slots == NULL || !string_map_key_valid(key)) {
        return false;
    }
    hash = stringv_hash(key);
    index = string_map_find_index(map, key, hash, NULL);
    if (index == SIZE_MAX) {
        return false;
    }
    string_map_slot_release(map, &map->slots[index]);
    map->slots[index].state = STRING_MAP_SLOT_TOMBSTONE;
    map->length--;
    map->tombstone_count++;
    return true;
}

void sm_clear(StringMap *map) {
    if (!string_map_initialized(map) || map->slots == NULL) {
        return;
    }
    for (size_t index = 0; index < map->capacity; index++) {
        if (map->slots[index].state == STRING_MAP_SLOT_OCCUPIED) {
            string_map_slot_release(map, &map->slots[index]);
        }
    }
    memset(map->slots, 0, map->capacity * sizeof(*map->slots));
    map->length = 0;
    map->tombstone_count = 0;
}

void sm_destroy(StringMap *map) {
    if (map == NULL) {
        return;
    }
    if (string_map_initialized(map)) {
        sm_clear(map);
        if (map->slots != NULL) {
            map->allocator.free(map->allocator.context, map->slots);
        }
    }
    memset(map, 0, sizeof(*map));
}

size_t sm_length(const StringMap *map) { return map == NULL ? 0 : map->length; }

StringMapIterator sm_iterator(void) { return (StringMapIterator){0}; }

bool sm_next_const(const StringMap *map, StringMapIterator *iterator,
                   StringView *key, const StringMapValue **value) {
    if (map == NULL || iterator == NULL || key == NULL || value == NULL) {
        return false;
    }
    while (iterator->index < map->capacity) {
        const StringMapSlot *slot = &map->slots[iterator->index++];

        if (slot->state == STRING_MAP_SLOT_OCCUPIED) {
            *key = slot->key;
            *value = &slot->value;
            return true;
        }
    }
    return false;
}

bool sm_next(StringMap *map, StringMapIterator *iterator, StringView *key,
             StringMapValue **value) {
    const StringMapValue *constant_value;

    if (value == NULL || !sm_next_const(map, iterator, key, &constant_value)) {
        return false;
    }
    *value = (StringMapValue *)constant_value;
    return true;
}
