#include "ceasy/string/string.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void assert_string(String *string, const char *expected) {
    assert(stringv_equal(string_as_view(string), stringv_from_cstr(expected)));
    assert(string->data != NULL);
    assert(string->data[string->length] == '\0');
}

static void test_views(void) {
    StringView value = sv("hello");
    StringView left;
    StringView right;
    size_t index;

    assert(value.length == 5);
    assert(value.data[0] == 'h' && value.data[4] == 'o');
    assert(sv("").length == 0);
    assert(stringv_from_cstr(NULL).length == 0);
    assert(stringv_empty((StringView){0}));
    assert(stringv_equal(value, sv("hello")));
    assert(!stringv_equal(value, sv("Hello")));
    assert(stringv_equal_ignore_case(sv("Content-Type"), sv("content-type")));
    assert(stringv_compare(sv("a"), sv("b")) < 0);
    assert(stringv_compare(sv("abc"), sv("abc")) == 0);
    assert(stringv_compare(sv("abc"), sv("abcd")) < 0);
    assert(stringv_starts_with(value, sv("he")));
    assert(stringv_starts_with(value, sv("")));
    assert(stringv_ends_with(value, sv("lo")));
    assert(stringv_contains(value, sv("ell")));
    assert(stringv_contains(value, sv("")));
    assert(stringv_find(value, sv("ll"), &index) && index == 2);
    assert(!stringv_find(value, sv("x"), &index) && index == SIZE_MAX);
    assert(stringv_find_char(value, 'h', &index) && index == 0);
    assert(stringv_rfind_char(sv("a-b-c"), '-', &index) && index == 3);
    assert(stringv_equal(stringv_slice(value, 1, 3), sv("ell")));
    assert(stringv_equal(stringv_slice(value, 4, 20), sv("o")));
    assert(stringv_slice(value, 9, 1).length == 0);
    assert(stringv_equal(stringv_trim(sv("\t hello\r\n")), sv("hello")));
    assert(stringv_trim(sv("   ")).length == 0);
    assert(stringv_split_once(sv("a:b:c"), sv(":"), &left, &right));
    assert(stringv_equal(left, sv("a")));
    assert(stringv_equal(right, sv("b:c")));
    assert(stringv_split_once_char(sv(":b"), ':', &left, &right));
    assert(left.length == 0 && stringv_equal(right, sv("b")));
    assert(!stringv_split_once(sv("abc"), sv(""), &left, &right));
    assert(stringv_equal(
        stringv_remove_prefix(sv("prefix-value"), sv("prefix-")), sv("value")));
    assert(stringv_equal(stringv_remove_suffix(sv("value.txt"), sv(".txt")),
                         sv("value")));
    assert(stringv_hash(sv("abc")) == stringv_hash(sv("abc")));
    assert(stringv_hash(sv("abc")) != stringv_hash(sv("abd")));
}

static void test_parsing(void) {
    int integer;
    int64_t integer64;
    size_t size;
    bool boolean;

    assert(stringv_parse_int(sv("0"), &integer) && integer == 0);
    assert(stringv_parse_int(sv("-42"), &integer) && integer == -42);
    assert(stringv_parse_int(sv("+42"), &integer) && integer == 42);
    assert(stringv_parse_int(sv("2147483647"), &integer));
    assert(!stringv_parse_int(sv("2147483648"), &integer));
    assert(stringv_parse_int64(sv("-9223372036854775808"), &integer64) &&
           integer64 == INT64_MIN);
    assert(stringv_parse_int64(sv("9223372036854775807"), &integer64));
    assert(!stringv_parse_int64(sv("9223372036854775808"), &integer64));
    assert(!stringv_parse_int(sv(" 1"), &integer));
    assert(!stringv_parse_int(sv("1x"), &integer));
    assert(stringv_parse_size(sv("12345"), &size) && size == 12345);
    assert(!stringv_parse_size(sv("-1"), &size));
    assert(stringv_parse_bool(sv("true"), &boolean) && boolean);
    assert(stringv_parse_bool(sv("0"), &boolean) && !boolean);
    assert(!stringv_parse_bool(sv("yes"), &boolean));
}

static void test_heap_string(void) {
    String string = string_new_heap();
    StringView own_view;

    assert(string.length == 0 && string.capacity == 0);
    assert(string_append(&string, sv("abc")));
    assert_string(&string, "abc");
    own_view = string_as_view(&string);
    assert(string_append(&string, own_view));
    assert_string(&string, "abcabc");
    assert(string_append_format(&string, "-%d-%s", 7, "ok"));
    assert_string(&string, "abcabc-7-ok");
    assert(string_prepend(&string, sv("start-")));
    assert_string(&string, "start-abcabc-7-ok");
    assert(string_copy(&string, sv("ABCxyz123")));
    string_upper(&string);
    assert_string(&string, "ABCXYZ123");
    string_lower(&string);
    assert_string(&string, "abcxyz123");
    assert(string_replace(&string, sv("xyz"), sv("-")));
    assert_string(&string, "abc-123");
    assert(string_replace_all(&string, sv("-"), sv("/")));
    assert_string(&string, "abc/123");
    assert(string_remove(&string, 3, 100));
    assert_string(&string, "abc");
    assert(string_reserve(&string, 100));
    assert(string.capacity >= 100);
    assert_string(&string, "abc");
    string_clear(&string);
    assert(string.length == 0 && string.capacity >= 100);
    assert_string(&string, "");
    string_destroy(&string);
    string_destroy(&string);
    assert(string.data == NULL && string.length == 0 && string.capacity == 0);
}

static void test_overlap_and_format(void) {
    String string = string_from_heap(sv("0123456789abcdefghij"));
    StringView substring = stringv_slice(string_as_view(&string), 3, 10);
    String formatted = string_format(allocator_heap(), "%s:%lld", "id", 42LL);

    assert(string_append(&string, substring));
    assert_string(&string, "0123456789abcdefghij3456789abc");
    assert(string_copy(&string, stringv_slice(string_as_view(&string), 2, 5)));
    assert_string(&string, "23456");
    assert(string_prepend(&string, string_as_view(&string)));
    assert_string(&string, "2345623456");
    assert_string(&formatted, "id:42");
    string_destroy(&string);
    string_destroy(&formatted);

    string = string_from_heap(sv("one two two"));
    assert(string_replace_all(&string, sv("two"), sv("2")));
    assert_string(&string, "one 2 2");
    string_destroy(&string);
}

typedef struct {
    bool fail_realloc;
} FailureAllocator;

static void *failure_alloc(void *context, size_t size) {
    (void)context;
    return malloc(size);
}

static void *failure_realloc(void *context, void *memory, size_t old_size,
                             size_t new_size) {
    FailureAllocator *failure = context;

    (void)old_size;
    if (failure->fail_realloc) {
        return NULL;
    }
    return realloc(memory, new_size);
}

static void failure_free(void *context, void *memory) {
    (void)context;
    free(memory);
}

static void test_failure_and_arena(void) {
    FailureAllocator failure = {0};
    Allocator allocator = {.context = &failure,
                           .alloc = failure_alloc,
                           .realloc = failure_realloc,
                           .free = failure_free};
    String string = string_from(allocator, sv("stable"));
    StringView before = string_as_view(&string);
    Arena arena;
    String arena_string;

    failure.fail_realloc = true;
    assert(!string_append(&string, sv("this forces growth")));
    assert(stringv_equal(string_as_view(&string), before));
    assert(string.data[string.length] == '\0');
    string_destroy(&string);

    assert(arena_init(&arena, 32));
    arena_string = string_new_in(&arena);
    for (size_t index = 0; index < 100; index++) {
        assert(string_append_char(&arena_string, (char)('a' + index % 26)));
    }
    assert(arena_string.length == 100);
    assert(arena_string.data[arena_string.length] == '\0');
    string_destroy(&arena_string);
    arena_destroy(&arena);
}

static void test_integration_flow(void) {
    Arena arena;
    StringView request = sv("GET /posts/123 HTTP/1.1");
    StringView method;
    StringView target;
    StringView path;
    StringView id;
    String response;
    int64_t post_id;

    assert(arena_init(&arena, 64));
    assert(stringv_split_once_char(request, ' ', &method, &target));
    assert(stringv_split_once_char(target, ' ', &path, &target));
    assert(stringv_equal(method, sv("GET")));
    assert(stringv_starts_with(path, sv("/posts/")));
    id = stringv_from(path, sv("/posts/").length);
    assert(stringv_parse_int64(id, &post_id) && post_id == 123);
    response = string_new_in(&arena);
    assert(string_append(&response, sv("<h1>Post ")));
    assert(string_append_format(&response, "%lld", post_id));
    assert(string_append(&response, sv("</h1>")));
    assert_string(&response, "<h1>Post 123</h1>");
    arena_destroy(&arena);
}

int main(void) {
    test_views();
    test_parsing();
    test_heap_string();
    test_overlap_and_format();
    test_failure_and_arena();
    test_integration_flow();
    return 0;
}
