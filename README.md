# build
docker build -t ceasy .

# run
docker run --rm -it -v "${PWD}:/workspace" -p 3000:3000 -w /workspace ceasy bash