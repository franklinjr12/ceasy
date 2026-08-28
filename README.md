# build
docker build -t ceasy .

# run
docker run --rm -it -v "${PWD}:/workspace" -p 3000:3000 -w /workspace ceasy bash

Seed development database inside container:

```bash
gcc tools/seed_database.c -lsqlite3 -o build/seed_database
./build/seed_database
```

Optional database path:

```bash
./build/seed_database db/test.sqlite3
```
