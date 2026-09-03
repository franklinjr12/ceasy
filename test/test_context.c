#include "ceasy/ceasy.h"

#include <assert.h>

int main(void) {
    Arena arena;
    Context context = {0};

    assert(arena_init(&arena, 128));
    context.arena = &arena;
    context.request.query_string = sv("page=2&q=hello+world&empty=&q=again");
    assert(context_parse_query(&context));
    assert(stringv_equal(context_query(&context, sv("page")), sv("2")));
    assert(stringv_equal(context_query(&context, sv("q")), sv("hello world")));
    assert(context_query(&context, sv("empty")).length == 0);
    context = (Context){.arena = &arena};
    context.request.query_string = sv("q=%ZZ");
    assert(!context_parse_query(&context));
    context = (Context){.arena = &arena};
    context.request.content_type =
        sv("application/x-www-form-urlencoded; charset=UTF-8");
    context.request.body = sv("title=Hello+World&content=a%26b&empty=");
    assert(context_parse_form(&context));
    assert(
        stringv_equal(context_form(&context, sv("title")), sv("Hello World")));
    assert(stringv_equal(context_form(&context, sv("content")), sv("a&b")));
    assert(context_form(&context, sv("empty")).length == 0);
    assert(context_form(&context, sv("missing")).length == 0);
    arena_destroy(&arena);
    return 0;
}
