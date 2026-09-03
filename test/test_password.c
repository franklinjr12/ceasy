#include <ceasy/ceasy.h>

#include <assert.h>
#include <string.h>

int main(void) {
    Arena arena;
    String digest;
    String second_digest;
    char oversized[1025];
    assert(arena_init(&arena, 4096));
    assert(password_hash(&arena, sv("long-enough-password"), &digest));
    assert(
        password_verify(string_as_view(&digest), sv("long-enough-password")));
    assert(!password_verify(string_as_view(&digest), sv("wrong-password")));
    assert(password_hash(&arena, sv("long-enough-password"), &second_digest));
    assert(!stringv_equal(string_as_view(&digest),
                          string_as_view(&second_digest)));
    memset(oversized, 'x', sizeof(oversized));
    assert(!password_hash(
        &arena, (StringView){.data = oversized, .length = sizeof(oversized)},
        &second_digest));
    arena_destroy(&arena);
    return 0;
}
