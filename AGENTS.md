# AGENTS.md

## Project Overview

Ceasy is an experimental web application framework written in C.

The project is inspired primarily by Ruby on Rails, especially its emphasis on:

* convention over configuration;
* productive defaults;
* MVC-style application structure;
* generators instead of repetitive boilerplate;
* server-side HTML rendering;
* simple routing and controller APIs;
* easy database access;
* migrations;
* strong developer ergonomics;
* predictable project structure.

Ceasy is intentionally unconventional: it aims to explore how pleasant a Rails-like web development experience can be when implemented in C.

The goal is not to reproduce Ruby or Rails internals. C does not provide Ruby's reflection, garbage collection, metaprogramming, or dynamic runtime.

Instead, reproduce the **developer experience and architectural ideas** of Rails using techniques that fit C well, especially:

* conventions;
* code generation;
* explicit metadata;
* request-scoped memory;
* simple public APIs;
* compile-time/build-time generation where Ruby would use runtime reflection.

---

# Development Philosophy

Ceasy should evolve through **vertical slices**.

Do not attempt to implement a complete low-level foundation before working on higher-level framework features.

Prefer building complete flows such as:

```text
HTTP request
    ↓
Router
    ↓
Controller
    ↓
SQLite
    ↓
Template
    ↓
HTML response
```

and improve the underlying String, Array, Map, Arena, database, and HTTP APIs as actual framework usage exposes their needs.

Foundational abstractions should be driven by real framework requirements.

Avoid speculative abstractions.

For example:

* add String functionality when HTTP, database, or templates need it;
* add Array functionality when application/framework code needs it;
* add Map functionality when routing, headers, params, or templates need it;
* improve Arena functionality as real request lifetimes expose requirements.

---

# Primary Product Principle

When implementing framework behavior, optimize for pleasant application code.

Application code should ideally resemble:

```c
void posts_index(Context *ctx)
{
    DbRows posts = db_query(
        ctx,
        "SELECT id, title FROM posts ORDER BY id DESC"
    );

    view_set(ctx, "posts", db_rows(posts));

    render(ctx, "posts/index");
}
```

Framework internals may be complex when necessary, but that complexity should not leak into normal application code.

Do not expose low-level details merely because Ceasy itself is written in C.

Application developers should rarely need to interact directly with:

```text
malloc/free
raw sockets
sqlite3_stmt
HTTP parser internals
template parser internals
framework allocator internals
```

---

# Current Architectural Direction

Ceasy is expected to evolve toward the following architecture:

```text
                    Application

          Controllers   Views   Models
                │         │       │
                └────┬────┴───────┘
                     │
                  Context
                     │
                  Router
                     │
             Request / Response
                     │
                HTTP Server

         ──────────────────────────

              Framework services

         Templates / Layouts
         SQLite / Queries
         Migrations
         Static Files
         Logging
         Generators

         ──────────────────────────

               Core utilities

         Arena
         String
         StringView
         Array
         Map
         Error
         Filesystem
```

Do not force this entire architecture to exist before implementing useful features.

Build toward it incrementally.

---

# Current Technology Decisions

Use:

```text
Language: C
Compiler: Clang
Database: SQLite
Rendering: server-side HTML
Frontend: HTML + CSS
Build/project tool: cdev
Environment: Docker
```

Initial HTTP implementation should remain intentionally simple.

Prefer:

```text
HTTP/1.1
single-threaded
synchronous
one request at a time initially
Connection: close
```

until real framework needs justify additional complexity.

Do not prematurely implement:

* HTTP/2;
* HTTP/3;
* WebSockets;
* TLS;
* asynchronous runtimes;
* event loops;
* thread pools;
* advanced streaming;
* distributed features.

---

# Docker Development Environment

All project commands must run through the repository Docker environment.

Do not rely on tools installed directly on the host machine.

The development image is built with:

```bash
docker build -t ceasy .
```

Start an interactive development shell with:

```bash
docker run --rm -it \
  -v "${PWD}:/workspace" \
  -w /workspace \
  ceasy bash
```

All builds, tests, formatting, generators, migrations, and other project commands must be run inside this Docker environment.

Do not instruct the user to install Clang, SQLite tooling, or other Ceasy development dependencies directly on the host unless explicitly asked.

For one-off automated execution, commands may also be run through Docker directly, for example:

```bash
docker run --rm \
  -v "${PWD}:/workspace" \
  -w /workspace \
  ceasy \
  ./bin/cdev build
```

Prefer using the existing Dockerfile instead of introducing alternate local development environments.

---

# `cdev`

Ceasy uses a custom C development tool called `cdev`.

The executable resides at:

```text
bin/cdev
```

Use:

```bash
./bin/cdev build
./bin/cdev run
./bin/cdev test
./bin/cdev format
./bin/cdev clean
```

inside the Docker environment.

Do not introduce:

```text
CMake
Meson
Make
Xmake
Ninja project definitions
```

unless explicitly requested.

`cdev` is the canonical Ceasy build/test/run/format workflow.

---

# `cdev` Project Conventions

`cdev` follows convention over configuration.

Application/framework C sources live beneath:

```text
src/
```

with arbitrary nested folders.

Examples:

```text
src/main.c
src/http/server.c
src/http/server.h
src/routing/router.c
src/routing/router.h
src/core/string/string.c
src/core/string/string.h
```

Tests live beneath:

```text
test/
```

with arbitrary nested folders.

Generated build artifacts live beneath:

```text
build/
```

Do not manually enumerate source files.

Adding:

```text
src/foo/bar.c
```

must be enough for `cdev` to discover it automatically.

Likewise, adding test files beneath:

```text
test/
```

must not require registration in a separate build definition.

---

# Build Workflow

Before considering implementation work complete, run:

```bash
./bin/cdev build
```

inside the Docker environment.

When modifying behavior, also run:

```bash
./bin/cdev test
```

When modifying C source formatting, run:

```bash
./bin/cdev format
```

A typical validation flow is:

```bash
./bin/cdev format
./bin/cdev build
./bin/cdev test
```

All of these commands must be executed inside the Ceasy Docker environment.

---

# Code Style

Use:

```text
snake_case
```

for:

* functions;
* variables;
* parameters;
* fields where appropriate;
* filenames;
* module names.

Examples:

```c
void posts_index(Context *ctx);

String request_param(Context *ctx, const char *name);

bool database_open(Database *database);
```

Use:

```text
CamelCase
```

for structs and typedef-defined types.

Examples:

```c
typedef struct {
    String method;
    String path;
} Request;

typedef struct {
    Request *request;
    Response *response;
    Arena *arena;
    Database *database;
} Context;
```

Constants and macros may use:

```text
UPPER_SNAKE_CASE
```

when appropriate.

Formatting is controlled through `clang-format` via:

```bash
./bin/cdev format
```

Do not manually introduce formatting styles inconsistent with the repository.

---

# C Style Guidelines

Prefer simple and readable C.

Avoid:

* macro-heavy pseudo-languages;
* deep preprocessor metaprogramming;
* fake object-oriented inheritance systems;
* unnecessary indirection;
* generic abstractions without concrete use cases;
* clever ownership tricks;
* hidden global mutable state.
* NEVER use goto or any jmp instructions.

Macros are acceptable when they provide meaningful framework ergonomics or metadata that C otherwise lacks.

Good uses may eventually include:

* generated declarations;
* model metadata;
* controller/action registration;
* typed collection generation;
* test helpers.

Macros should not obscure control flow unnecessarily.

---

# Memory Management

Memory management is a major experimental area of Ceasy.

A primary goal is to make application-level memory management much more pleasant than raw C.

The expected default model is:

```text
long-lived framework resources
    → explicit ownership and cleanup

request-scoped temporary resources
    → request Arena
```

Each HTTP request should eventually have a request-scoped arena.

Typical request-lifetime allocations include:

* parsed path values;
* query parameters;
* form parameters;
* temporary strings;
* database row data;
* template data;
* rendered HTML;
* route parameters.

The conceptual lifecycle is:

```text
request received
    ↓
arena created
    ↓
routing/controller/database/rendering
    ↓
response sent
    ↓
arena destroyed
```

Prefer stack allocation where practical.

Avoid scattered `malloc()` / `free()` calls throughout controllers and application-facing APIs.

When heap ownership is necessary, ownership must be obvious from the API.

Document whether returned values are:

* borrowed;
* arena-owned;
* caller-owned;
* framework-owned.

Do not return ambiguous allocations.

---

# String APIs

Ceasy will experiment with better C string APIs.

Likely core types include:

```c
typedef struct {
    char *data;
    size_t length;
} String;

typedef struct {
    const char *data;
    size_t length;
} StringView;
```

General intent:

```text
String
    owned or arena-backed string data

StringView
    borrowed non-owning slice
```

Prefer length-aware string handling over assuming null-terminated strings everywhere.

Use `StringView` where parsing can safely reference existing buffers without copying.

Do not prematurely create a huge string standard library.

Add operations based on real requirements from:

* HTTP;
* routing;
* query parameters;
* database access;
* templates;
* HTML escaping;
* filesystem operations.

---

# Arrays and Collections

Ceasy will experiment with practical C collection APIs.

Do not attempt to implement a complete generic collection framework immediately.

Prefer concrete needs first.

Likely requirements include:

* push;
* length;
* iteration;
* capacity growth;
* arena-backed arrays;
* typed arrays through simple macros or generated code.

Application-facing APIs should prefer typed collections where practical.

For example:

```c
PostArray posts;
```

is preferable to exposing untyped `void **` structures throughout application code.

---

# Maps / Hashes

Maps will be needed for:

* HTTP headers;
* route parameters;
* query parameters;
* form parameters;
* template data;
* configuration.

Start with the simplest useful map types.

A:

```text
String → String
```

map will cover many early framework needs.

Do not implement arbitrary generic hash structures before a concrete framework requirement exists.

---

# Error Handling

Avoid pretending C has exceptions.

Do not build complex TRY/CATCH macro systems unless future usage clearly demonstrates a strong benefit.

Prefer:

* explicit return values;
* explicit error objects;
* context-owned errors;
* clear failure propagation.

Framework errors should eventually distinguish concepts such as:

```text
not found
bad request
database error
template error
internal error
```

Do not hide compiler errors, SQLite errors, or operating system errors when they provide useful diagnostics.

---

# HTTP Layer

The HTTP subsystem should be internally separated from application-facing APIs.

Possible internal modules:

```text
src/http/server.c
src/http/request.c
src/http/response.c
src/http/parser.c
```

Application code should not need to understand socket handling.

Early supported behavior should focus on:

```text
GET
POST
basic headers
Content-Length
request body
query string
HTTP status
response headers
response body
```

Add methods such as:

```text
PATCH
PUT
DELETE
```

when CRUD flows require them.

Do not parse more of HTTP than the framework currently needs.

---

# Routing

Routing should provide a simple application-facing API.

Expected direction:

```c
void routes(Router *router)
{
    route_get(router, "/", home_index);
    route_get(router, "/posts", posts_index);
    route_get(router, "/posts/:id", posts_show);
    route_post(router, "/posts", posts_create);
}
```

Later Ceasy may introduce helpers such as:

```c
resources(router, "posts", posts_controller);
```

Do not implement Rails-style resource abstractions before normal explicit routing is stable.

Route parameters such as:

```text
/posts/:id
```

should eventually be available through the request/context API.

---

# Controllers

Controller actions should remain ordinary C functions.

Prefer:

```c
void posts_index(Context *ctx);
```

Do not introduce fake inheritance hierarchies such as controller structs extending other controller structs unless there is a concrete need.

`Context` is the stable application-facing request boundary.

Expected responsibilities may include:

```c
typedef struct {
    Request *request;
    Response *response;
    Arena *arena;
    Database *database;

    /* future */
    /* session */
    /* logger */
    /* route metadata */
    /* controller/action metadata */
    /* view data */
} Context;
```

Prefer extending `Context` over continually changing controller function signatures.

---

# Rendering

Ceasy is primarily a server-rendered HTML framework.

Application APIs should evolve toward:

```c
render(ctx, "posts/index");
redirect_to(ctx, "/posts");
render_text(ctx, "Hello");
render_html(ctx, html);
render_not_found(ctx);
```

Framework internals should own response serialization.

Controllers should not manually construct raw HTTP responses unless using intentionally low-level APIs.

---

# Templates

Ceasy should use a small server-side template engine.

Do not attempt to implement an embedded C interpreter or a complex expression language initially.

Expected early features:

```text
{{ variable }}

{{#collection}}
...
{{/collection}}

{{?condition}}
...
{{/condition}}

{{> partial }}
```

HTML escaping must be the default.

Unsafe/raw HTML output must require explicit syntax or API usage.

Security-sensitive defaults should be safe by convention.

---

# Layouts

The default layout should follow convention:

```text
views/layouts/application.html
```

A controller rendering:

```text
posts/index
```

should normally resolve:

```text
views/posts/index.html
```

and insert its output into the application layout automatically.

Avoid requiring controllers to specify default view or layout paths repeatedly.

---

# Partials

Expected convention:

```text
views/posts/_post.html
```

corresponds to a logical partial such as:

```text
posts/post
```

Do not implement unnecessary partial complexity before templates themselves are stable.

---

# Static Files

Files beneath:

```text
public/
```

should eventually be served directly.

Example:

```text
public/styles/application.css
```

maps to:

```text
/styles/application.css
```

No asset pipeline is required initially.

Do not introduce:

* JavaScript bundlers;
* CSS preprocessors;
* fingerprinting;
* asset compilation;

until there is a demonstrated need.

HTML/CSS editing should ideally require only a browser refresh in development.

---

# SQLite

SQLite is the default and initial database.

Use SQLite's normal C API internally.

Application-facing APIs should hide repetitive:

```text
sqlite3_prepare_v2
sqlite3_bind_*
sqlite3_step
sqlite3_column_*
sqlite3_finalize
```

usage.

Early Ceasy database APIs may be relatively low level.

That is acceptable.

Allow the API to evolve through actual framework usage.

Expected progression:

```text
SQLite wrapper
    ↓
Query / Row API
    ↓
typed/generated model mapping
    ↓
Active Record-like convenience APIs
```

Do not start by attempting to recreate the entirety of Rails Active Record.

---

# Database Queries

Prepared statements and parameter binding should be preferred.

Do not build queries by concatenating untrusted request values.

Application-facing APIs should eventually support patterns like:

```c
DbQuery query = db_query(
    ctx,
    "SELECT id, title FROM posts WHERE id = ?",
    db_int(id)
);
```

SQL injection prevention should be the default behavior.

---

# Migrations

Ceasy should support migrations relatively early.

Prefer SQL migration files initially.

Example:

```text
db/migrations/
20260827210000_create_posts.sql
20260827211000_add_published_to_posts.sql
```

Use a SQLite table such as:

```text
schema_migrations
```

to track applied migrations.

Do not create a C migration DSL until real experience demonstrates that SQL files are insufficient.

---

# Models

Models should initially be ordinary C structs plus explicit/generated persistence functions.

Example:

```c
typedef struct {
    int64_t id;
    String title;
    String body;
} Post;
```

Expected eventual APIs may resemble:

```c
Post *post_find(Context *ctx, int64_t id);
PostArray post_all(Context *ctx);
bool post_save(Context *ctx, Post *post);
bool post_destroy(Context *ctx, Post *post);
```

Do not attempt runtime reflection.

C does not provide it naturally.

Use code generation and explicit metadata instead.

---

# Code Generation

Code generation is a **core architectural strategy** in Ceasy.

Prefer generators where Rails would normally rely on dynamic reflection, conventions, or repetitive boilerplate.

Expected future CLI examples:

```bash
ceasy generate controller posts index show new create

ceasy generate model Post title:string body:text

ceasy generate scaffold Post title:string body:text
```

Generators may create:

* controller `.c` / `.h` files;
* model `.c` / `.h` files;
* views;
* migrations;
* tests;
* route metadata;
* generated registration code;
* model field metadata;
* serialization/mapping helpers.

Use generated C code when it provides clearer runtime behavior than complicated macro systems.

Generated code should still be understandable C whenever practical.

---

# Generator Philosophy

Do not invent generator output before the hand-written equivalent has stabilized.

Preferred process:

```text
implement feature manually
    ↓
use it in example application
    ↓
identify repeated convention
    ↓
design generator
    ↓
generate the established pattern
```

Generators should encode proven framework conventions rather than speculative designs.

---

# Generated Code

Generated code that exists only as a build artifact should live under:

```text
build/generated/
```

Generated application code intended for the developer to edit may live under:

```text
src/
views/
db/
test/
```

Clearly distinguish between:

```text
generated implementation detail
```

and:

```text
generated application starter code
```

Do not silently overwrite user-edited application files.

---

# Reflection Strategy

Ruby on Rails can discover substantial application metadata at runtime.

Ceasy should generally replace runtime reflection with:

```text
convention
+
code generation
+
explicit metadata
```

For example, a generator may produce action registration such as:

```c
register_action(
    app,
    "posts",
    "index",
    posts_index
);
```

This is preferable to implementing brittle source-code parsing at runtime.

---

# Public vs Internal API

Maintain a clear boundary from the beginning.

Application developers should include public headers such as:

```c
#include <ceasy/ceasy.h>
```

They should not need to include internals such as:

```text
http/parser_internal.h
database/sqlite_internal.h
template/parser_internal.h
```

Internal APIs may change freely during early development.

Public APIs should be intentionally designed.

Do not expose internals simply because doing so is convenient during implementation.

---

# Example Application

Maintain a small real application under something like:

```text
examples/blog/
```

Use it as:

* a framework playground;
* an integration test;
* a design feedback mechanism;
* a demonstration of expected Ceasy usage.

Grow the application alongside the framework.

Expected evolution:

```text
GET /
    ↓
GET /posts
    ↓
SQLite-backed posts
    ↓
templates
    ↓
layouts
    ↓
static CSS
    ↓
GET /posts/:id
    ↓
GET /posts/new
    ↓
POST /posts
    ↓
edit/update/delete
    ↓
generated models
    ↓
generated scaffolds
```

Framework abstractions should be tested against real application code.

---

# Development Sequence

Prefer complete vertical slices over layer-complete implementation.

A useful current implementation progression is:

```text
Ceasy boots
    ↓
GET /
    ↓
router
    ↓
controller Context
    ↓
GET /posts
    ↓
SQLite query
    ↓
HTML response
    ↓
template
    ↓
layout
    ↓
static CSS
    ↓
/posts/:id
    ↓
route params
    ↓
new/create
    ↓
POST parsing
    ↓
redirect
```

Do not spend large amounts of time perfecting low-level systems if doing so blocks visible framework progress.

---

# Testing

Testing is required for framework functionality.

Use:

```bash
./bin/cdev test
```

inside Docker.

Add automated tests for:

* String behavior;
* StringView behavior;
* Array behavior;
* Map behavior;
* Arena behavior;
* HTTP parsing;
* request parsing;
* response serialization;
* route matching;
* route parameters;
* query parameter parsing;
* form parsing;
* template parsing;
* HTML escaping;
* SQLite wrapper behavior;
* migrations;
* generated metadata;
* generators.

Prefer regression tests for bugs.

---

# Integration Testing

Unit tests alone are not enough.

Add integration tests for real flows such as:

```text
GET /posts
```

and verify:

```text
HTTP status
response headers
rendered body
database data
```

SQLite in-memory databases are encouraged for isolated tests where appropriate:

```text
:memory:
```

The example application may also serve as an end-to-end fixture.

---

# Testability

Keep components independently testable.

Good boundaries include:

```text
HTTP parser
Router
Template engine
Database wrapper
Migration runner
Generator
```

Avoid binding every subsystem directly to the network server.

For example, routing/controller tests should eventually be able to create synthetic requests without opening real TCP sockets.

---

# Logging

Development logging should eventually resemble productive web framework output.

Example:

```text
GET /posts
→ posts#index
SQL SELECT id, title FROM posts
VIEW views/posts/index.html
200 OK 2.3ms
```

Keep logs useful and concise.

Do not create a complicated logging framework prematurely.

---

# Security Defaults

Even though Ceasy is experimental, avoid teaching unsafe patterns.

Important defaults:

* HTML escape template values;
* use prepared SQL statements;
* validate static file paths against traversal;
* bound HTTP parsing where appropriate;
* reject malformed request sizes;
* avoid shell execution for user-controlled values;
* avoid unsafe string functions where practical.

Do not sacrifice obvious safety requirements for convenience.

---

# Performance

Correctness and API quality matter more than optimization early in development.

Do not introduce:

* custom lock-free data structures;
* complex zero-copy systems;
* advanced async networking;
* specialized allocators;

without demonstrated need.

Reasonable optimizations are welcome when they also simplify ownership or design, such as using `StringView` to avoid unnecessary parsing copies.

---

# Framework Scope

Ceasy is currently focused on traditional server-rendered applications.

Prioritize:

```text
HTML
CSS
forms
CRUD
SQLite
controllers
views
models
routing
migrations
generators
```

Do not prioritize SPA-oriented APIs, GraphQL, distributed services, or microservice architecture.

JSON support may eventually be useful but is not the primary framework identity.

---

# Convention Over Configuration

Before introducing configuration, ask:

> Can Ceasy infer this safely from project structure or naming?

Examples of preferred conventions:

```text
Posts controller
    → posts_controller.c

posts#index
    → views/posts/index.html

default layout
    → views/layouts/application.html

Post
    → posts table

development database
    → db/development.sqlite3

test database
    → :memory: or db/test.sqlite3

static assets
    → public/
```

Only add configuration when a legitimate override is required.

Avoid turning Ceasy into a framework that requires large configuration files before serving a page.

---

# Environments

Ceasy will eventually distinguish:

```text
development
test
production
```

Expected behaviors may include:

### Development

* verbose errors;
* request logging;
* templates reloaded frequently;
* `db/development.sqlite3`.

### Test

* isolated database;
* minimal noise;
* deterministic behavior.

### Production

* generic error pages;
* template caching;
* optimized execution;
* `db/production.sqlite3`.

Do not build a large environment configuration system prematurely.

---

# Application Experience Goal

Long term, Ceasy should be moving toward a workflow such as:

```bash
ceasy new blog
cd blog

ceasy generate scaffold Post title:string body:text

ceasy db:migrate

./bin/cdev run
```

resulting in a functional server-rendered CRUD application.

This is a guiding product target, not a requirement for early milestones.

---

# Architecture Review Questions

Before introducing a significant abstraction, ask:

1. Does current application code actually need this?
2. Does it improve the application-facing API?
3. Can convention solve it instead?
4. Can generation solve it instead?
5. Does this expose framework internals unnecessarily?
6. Is ownership clear?
7. Can it be tested independently?
8. Are we adding complexity to solve a hypothetical future problem?
9. Would normal Ceasy application code become simpler because of this?
10. Are we accidentally rebuilding a general-purpose C library instead of solving Ceasy's needs?

---

# Codex Rules

When working on Ceasy:

1. Read the existing implementation before designing replacements.

2. Preserve working conventions unless there is a concrete reason to change them.

3. Prefer incremental vertical progress.

4. Do not build entire speculative foundation layers.

5. Keep application-facing APIs small and pleasant.

6. Add tests for meaningful behavior.

7. Add regression tests when fixing bugs.

8. Run formatting after modifying C files.

9. Run build and tests before considering work complete.

10. Use `./bin/cdev` as the canonical build/test/run/format tool.

11. Execute all development commands inside the repository Docker environment.

12. Do not introduce CMake, Meson, or another build system.

13. Do not introduce third-party dependencies without strong justification.

14. Prefer libc, POSIX APIs, SQLite, Clang, and existing Ceasy utilities.

15. Prefer code generation over complicated runtime reflection emulation.

16. Prefer code generation over extreme macro metaprogramming.

17. Do not parse C source code unless there is no cleaner metadata/generation solution.

18. Keep public and internal framework APIs separated.

19. Preserve raw compiler/SQLite/system diagnostics when useful.

20. Avoid hidden ownership.

21. Keep request-lifetime allocations compatible with the Arena strategy.

22. Prefer simple C over clever C.

23. Avoid global mutable framework state when a context or explicit owner can be used.

24. Keep modules focused on a clear responsibility.

25. Do not optimize prematurely.

26. Preserve convention over configuration.

27. Avoid configuration until convention cannot reasonably solve the requirement.

28. Use the example application to validate framework ergonomics.

29. When an API feels repetitive, first understand the repeated real-world pattern before abstracting it.

30. Remember that Ceasy is both a usable framework experiment and a reference project for experimenting with better C ergonomics.

---

# Required Validation

For normal C changes, execute inside the Docker environment:

```bash
./bin/cdev format
./bin/cdev build
./bin/cdev test
```

The Docker environment itself is created using:

```bash
docker build -t ceasy .
```

and entered using:

```bash
docker run --rm -it \
  -v "${PWD}:/workspace" \
  -w /workspace \
  ceasy bash
```

Do not report implementation work as complete when relevant build or test failures remain.

If tests cannot be run for a legitimate environmental reason, clearly state what was not validated and why.

---

# Guiding Principle

Ceasy should make writing a conventional web application in C feel surprisingly pleasant.

When choosing between two designs, prefer the one that lets application code look more like:

```c
void posts_show(Context *ctx)
{
    int64_t id = param_int(ctx, "id");

    Post *post = post_find(ctx, id);

    view_set(ctx, "post", post);
}
```

and less like:

```c
void posts_show(...)
{
    /* socket management */
    /* buffer allocation */
    /* raw sqlite statement handling */
    /* manual response serialization */
    /* scattered malloc/free cleanup */
}
```

The framework exists to absorb that complexity.

Ceasy should remain recognizably C while borrowing Rails's strongest idea:

> sensible conventions should make the common path extremely easy.
