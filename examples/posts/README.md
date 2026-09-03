# Ceasy Journal

Ceasy Journal is a small multi-user publishing application. It exercises
registration, password hashing, server-side sessions, CSRF-protected forms,
authored drafts, publishing, comments, search, pagination, and authorization.

Migrate, compile views, build, and seed inside Ceasy's Docker environment:

```bash
../../bin/ceasy db:migrate
../../bin/ceasy views:compile
docker run --rm -v "${PWD}:/workspace" -w /workspace/examples/posts ceasy \
  bash -c 'mkdir -p build && clang tools/seed_database.c -lsqlite3 -o build/seed_database && ./build/seed_database'
../../bin/cdev build
```

The seed account is `admin@ceasy.local` with password `ceasy-development`.
The seed tool also creates a second reader account and sample articles.

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

* `http://localhost:3000/`
* `http://localhost:3000/posts`
* `http://localhost:3000/signup`

All unsafe browser forms include CSRF tokens. Production configuration uses
`CEASY_DATABASE_PATH`, `CEASY_PORT`, and Secure, HttpOnly session cookies.

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

Production container
--------------------

Build the production-oriented image from the repository root:

```bash
docker build -f examples/posts/Dockerfile.production -t ceasy-journal .
```

Run migrations as a release step, then start the image with a persistent data
volume. The production cookie is Secure, so place a TLS-terminating Caddy or
nginx reverse proxy in front of Ceasy:

```text
Internet -> Caddy/nginx -> Ceasy :3000 -> /data/blog.sqlite3
```

The runtime image contains only the compiled Posts executable, SQLite/libsodium
runtime libraries, and the writable `/data` volume. Do not run migrations from
inside request workers.
