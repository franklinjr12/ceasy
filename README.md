# build
docker build -t ceasy .

# run
docker run --rm -it -v "${PWD}:/workspace" -p 3000:3000 -w /workspace ceasy bash

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

Define `routes` in application code. The weak default serves `/` and `/posts`.

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
raw HTTP request is available in `context->request`.
