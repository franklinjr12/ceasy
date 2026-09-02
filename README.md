# build
docker build -t ceasy .

# run
docker run --rm -it -v "${PWD}:/workspace" -p 3000:3000 -w /workspace ceasy bash

Ceasy builds as a library. Posts sample app uses it through cdev dependency:

```bash
./bin/cdev build
cd examples/posts
../../bin/cdev build
```

Seed and run Posts:

```bash
mkdir -p db build
clang tools/seed_database.c -lsqlite3 -o build/seed_database
./build/seed_database
../../bin/cdev run
```

`GET /posts` returns seeded posts.

Arena memory
------------

`<ceasy/memory/arena.h>` provides grouped, request-friendly ownership without
individual frees. Initialize with a preferred block size; allocations grow into
stable linked blocks and return `NULL` on zero-size requests or allocation
failure. `arena_reset` reuses normal blocks, `arena_restore` rolls back to a
mark, and `arena_destroy` releases all blocks. Allocation pointers become
invalid after a reset/restore that covers them or after destruction. Arena
instances require external synchronization.

```c
typedef struct {
    int id;
} User;

Arena arena;
arena_init(&arena, 64 * 1024);

User *user = arena_new_zero(&arena, User);
ArenaMark mark = arena_mark(&arena);
void *temporary = arena_alloc(&arena, 4096);
(void)temporary;
arena_restore(&arena, mark);
arena_destroy(&arena);
```

Strings
-------

`StringView` is a non-owning, length-aware byte slice. It may not be
null-terminated and never frees its data. Use `sv("literal")` for literals;
use `stringv_from_cstr` for runtime C strings. `String` owns mutable,
null-terminated storage through its selected allocator.

```c
Arena arena;
arena_init(&arena, 4096);

String html = string_new_in(&arena);
string_append(&html, sv("<h1>"));
string_append(&html, sv("Ceasy"));
string_append(&html, sv("</h1>"));

arena_destroy(&arena);
```

StringView parsing is byte-oriented and does not silently trim whitespace.
`stringv_equal_ignore_case`, `string_upper`, and `string_lower` use ASCII
semantics. Arena-backed String memory is reclaimed by `arena_destroy`; heap
Strings release storage with `string_destroy`.

Seed development database inside container:

```bash
clang tools/seed_database.c -lsqlite3 -o build/seed_database
./build/seed_database
```

Optional database path:

```bash
./build/seed_database db/test.sqlite3
```

Routes
------

Define `routes` in application code. The weak default serves `/`.

```c
void routes(Router *router)
{
    route_get(router, "/", home_index);
    route_get(router, "/posts/:id", posts_show);
    route_post(router, "/posts", posts_create);
}
```

Route paths support static segments and `:name` parameter segments. Each
request is handled in a child process and delivered through `Context`, whose
structured request is available in `context->request`. `context_form()` parses
URL-encoded forms; `context_param()` reads route parameters.
