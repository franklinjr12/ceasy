#ifndef CEASY_COLLECTION_STRING_MAP_H
#define CEASY_COLLECTION_STRING_MAP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <ceasy/memory/allocator.h>
#include <ceasy/string/string.h>

typedef enum {
    SM_VALUE_EMPTY = 0,
    SM_VALUE_INT64,
    SM_VALUE_BOOL,
    SM_VALUE_STRING,
    SM_VALUE_POINTER,
    SM_VALUE_OBJECT
} StringMapValueType;

typedef struct {
    void *data;
    size_t size;
} StringMapObject;

typedef struct {
    StringMapValueType type;
    bool valid;
    bool owns_data;
    union {
        int64_t integer;
        bool boolean;
        StringView string;
        void *pointer;
        StringMapObject object;
    };
} StringMapValue;

typedef enum {
    STRING_MAP_SLOT_EMPTY = 0,
    STRING_MAP_SLOT_OCCUPIED,
    STRING_MAP_SLOT_TOMBSTONE
} StringMapSlotState;

typedef struct {
    uint64_t hash;
    StringView key;
    StringMapValue value;
    StringMapSlotState state;
    bool owns_key;
} StringMapSlot;

typedef struct {
    Allocator allocator;
    StringMapSlot *slots;
    size_t capacity;
    size_t length;
    size_t tombstone_count;
} StringMap;

typedef struct {
    size_t index;
} StringMapIterator;

/* Keys and StringMapValue strings/objects are copied by the normal setters.
 * sm_set_value_borrowed_key and sm_string_borrowed leave their data owned by
 * the caller. Pointer values are always borrowed and object copies are
 * shallow byte-for-byte copies. Map lookups and iterators are invalidated by
 * mutations that can resize or replace the map. Iteration order is undefined.
 */

bool sm_init(StringMap *map, Allocator allocator);
bool sm_init_in(StringMap *map, Arena *arena);
bool sm_init_heap(StringMap *map);

bool sm_set_value(StringMap *map, StringView key, StringMapValue value);
bool sm_set_value_borrowed_key(StringMap *map, StringView key,
                               StringMapValue value);

bool sm_set_int(StringMap *map, StringView key, int value);
bool sm_set_long(StringMap *map, StringView key, long value);
bool sm_set_long_long(StringMap *map, StringView key, long long value);
bool sm_set_bool(StringMap *map, StringView key, bool value);
bool sm_set_string_view(StringMap *map, StringView key, StringView value);

#define sm_set(map, key, value)                                                \
    _Generic((value), int                                                      \
             : sm_set_int, long                                                \
             : sm_set_long, long long                                          \
             : sm_set_long_long, bool                                          \
             : sm_set_bool, StringView                                         \
             : sm_set_string_view, StringMapValue                              \
             : sm_set_value)((map), (key), (value))

StringMapValue sm_int(int64_t value);
StringMapValue sm_bool(bool value);
StringMapValue sm_string_copy(StringMap *map, StringView value);
StringMapValue sm_string_borrowed(StringView value);
StringMapValue sm_pointer(void *pointer);
StringMapValue sm_object(StringMap *map, const void *data, size_t size);

#define sm_struct(map, value) sm_object((map), &(value), sizeof(value))

StringMapValue *sm_get(StringMap *map, StringView key);
const StringMapValue *sm_get_const(const StringMap *map, StringView key);
bool sm_get_int(const StringMap *map, StringView key, int64_t *result);
bool sm_get_bool(const StringMap *map, StringView key, bool *result);
bool sm_get_string(const StringMap *map, StringView key, StringView *result);
bool sm_get_pointer(const StringMap *map, StringView key, void **result);
void *sm_get_object(StringMap *map, StringView key, size_t expected_size);
const void *sm_get_object_const(const StringMap *map, StringView key,
                                size_t expected_size);

bool sm_contains(const StringMap *map, StringView key);
bool sm_remove(StringMap *map, StringView key);
void sm_clear(StringMap *map);
void sm_destroy(StringMap *map);
size_t sm_length(const StringMap *map);

StringMapIterator sm_iterator(void);
bool sm_next(StringMap *map, StringMapIterator *iterator, StringView *key,
             StringMapValue **value);
bool sm_next_const(const StringMap *map, StringMapIterator *iterator,
                   StringView *key, const StringMapValue **value);

#endif
