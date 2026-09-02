# Posts example

Posts is a small Ceasy application. It uses Ceasy as a local library dependency
and exposes browser CRUD for SQLite-backed posts.

Migrate, compile views, build, and seed inside Ceasy's Docker environment:

```bash
../../bin/ceasy db:migrate
../../bin/ceasy views:compile
docker run --rm -v "${PWD}:/workspace" -w /workspace/examples/posts ceasy \
  bash -c 'mkdir -p build && clang tools/seed_database.c -lsqlite3 -o build/seed_database && ./build/seed_database'
../../bin/cdev build
```

During development, `views/**/*.html` is read on each request, so refreshing
the browser shows template changes without rebuilding the application. For a
production-like single executable, run `../../bin/ceasy views:compile` before
`../../bin/cdev build`; the generated `src/generated/ceasy_views.c` is
disposable build output and embeds the parsed templates.

Run app:

```bash
docker run --rm -it -p 3000:3000 \
  -v "${PWD}:/workspace" -w /workspace/examples/posts ceasy ../../bin/cdev run
```

Browse:

* `http://localhost:3000/posts`
* `http://localhost:3000/posts/new`

Links and forms provide show, edit, update, and delete. Browser forms use
POST method override for PATCH and DELETE. Forms are intentionally simple and
have no CSRF protection yet.

Static assets
-------------

During development, `views/` and `public/` are read directly from disk on
each request. CSS and JavaScript changes are visible after a browser refresh.
The layout uses ordinary `/styles/application.css` and
`/scripts/application.js` URLs; Ceasy does not require asset helper syntax.

For a production-like single executable, compile both views and assets before
building:

```bash
../../bin/ceasy views:compile
../../bin/ceasy assets:compile
../../bin/cdev build
```

The generated C bundles are disposable and let the application run without
the `views/` or `public/` directories.
