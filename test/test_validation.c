#include <ceasy/ceasy.h>

#include <assert.h>

int main(void) {
    Arena arena;
    ValidationErrors errors;
    ViewValue view;

    assert(arena_init(&arena, 4096));
    validation_errors_init(&errors, &arena);
    assert(validation_present(sv("Ada")));
    assert(!validation_present(sv("   ")));
    assert(validation_length_between(sv("abc"), 2, 4));
    assert(!validation_length_between(sv("a"), 2, 4));
    assert(validation_length_at_most(sv("abc"), 3));
    assert(validation_equal(sv("x"), sv("x")));
    assert(!validation_equal(sv("x"), sv("y")));
    assert(validation_email_like(sv("ada@example.com")));
    assert(validation_email_like(sv("ada@example")));
    assert(!validation_email_like(sv("@example.com")));
    assert(!validation_email_like(sv("ada@")));
    assert(validation_errors_add(&errors, sv("email"), sv("Invalid email.")));
    assert(validation_errors_add(&errors, sv("name"), sv("Name required.")));
    assert(validation_errors_any(&errors));
    assert(validation_errors_count(&errors) == 2);
    assert(stringv_equal(validation_error_for(&errors, sv("email")),
                         sv("Invalid email.")));
    view = validation_errors_view(&errors);
    assert(view.type == VIEW_VALUE_COLLECTION);
    assert(view.as.collection.length == 2);
    arena_destroy(&arena);
    return 0;
}
