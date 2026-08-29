# Posts example

Posts is a small Ceasy application. It uses Ceasy as a local library dependency
and exposes `GET /posts`.

Build and seed inside Ceasy's Docker environment:

```bash
docker build -t ceasy .
docker run --rm -v "${PWD}:/workspace" -w /workspace/examples/posts ceasy \
  bash -c 'mkdir -p db build && clang tools/seed_database.c -lsqlite3 -o build/seed_database && ./build/seed_database && ../../bin/cdev build'
```

Run app:

```bash
docker run --rm -it -p 3000:3000 \
  -v "${PWD}:/workspace" -w /workspace/examples/posts ceasy ../../bin/cdev run
```
