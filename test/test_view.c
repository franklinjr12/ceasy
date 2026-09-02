#define _XOPEN_SOURCE 700

#include <ceasy/ceasy.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

extern char *mkdtemp(char *template);

typedef struct {
    String name;
    int64_t score;
    bool active;
} TestViewRecord;

static const ModelField test_view_fields[] = {
    {.name = sv("name"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(TestViewRecord, name)},
    {.name = sv("score"),
     .type = MODEL_FIELD_INT64,
     .offset = offsetof(TestViewRecord, score)},
    {.name = sv("active"),
     .type = MODEL_FIELD_BOOL,
     .offset = offsetof(TestViewRecord, active)},
};

static const ModelDefinition test_view_definition = {
    .name = sv("TestViewRecord"),
    .size = sizeof(TestViewRecord),
    .fields = test_view_fields,
    .field_count = sizeof(test_view_fields) / sizeof(test_view_fields[0]),
};

static const TemplateInstruction embedded_instructions[] = {
    {.type = TEMPLATE_INSTRUCTION_TEXT, .value = sv("embedded")},
};

static const EmbeddedView embedded_views[] = {
    {.path = sv("embedded"),
     .source_name = sv("embedded.html"),
     .instructions = embedded_instructions,
     .instruction_count = 1},
};

static const ViewBundle embedded_bundle = {
    .views = embedded_views,
    .view_count = 1,
};

const ViewBundle *ceasy_view_bundle(void) { return &embedded_bundle; }

static void write_file(const char *path, const char *contents) {
    FILE *file = fopen(path, "wb");

    assert(file != NULL);
    assert(fwrite(contents, 1, strlen(contents), file) == strlen(contents));
    assert(fclose(file) == 0);
}

static void setup_views(void) {
    assert(mkdir("views", 0755) == 0);
    assert(mkdir("views/posts", 0755) == 0);
    assert(mkdir("views/layouts", 0755) == 0);
    write_file("views/index.html", "{{#records}}{{> posts/record}}{{/records}}"
                                   "{{^records}}empty{{/records}}");
    write_file("views/posts/_record.html", "{{name}}/{{page_title}};");
    write_file("views/layouts/application.html",
               "<title>{{page_title}}</title>{{yield}}");
}

static void cleanup_views(void) {
    assert(unlink("views/index.html") == 0);
    assert(unlink("views/missing.html") == 0);
    assert(unlink("views/posts/_record.html") == 0);
    assert(unlink("views/layouts/application.html") == 0);
    assert(rmdir("views/posts") == 0);
    assert(rmdir("views/layouts") == 0);
    assert(rmdir("views") == 0);
}

static void test_parser(void) {
    Arena arena;
    TemplateProgram program = {0};
    TemplateError error = {0};

    assert(arena_init(&arena, 256));
    assert(template_parse(&arena, sv("Hello {{name}}{{#items}}x{{/items}}"),
                          sv("test.html"), &program, &error));
    assert(program.instruction_count == 4);
    assert(program.instructions[0].type == TEMPLATE_INSTRUCTION_TEXT);
    assert(program.instructions[1].type == TEMPLATE_INSTRUCTION_VARIABLE);
    assert(program.instructions[2].type == TEMPLATE_INSTRUCTION_SECTION);
    assert(program.instructions[2].jump == 4);
    template_error_destroy(&error);
    assert(!template_parse(&arena, sv("{{#items}}x{{/other}}"), sv("test.html"),
                           &program, &error));
    assert(error.line == 1 && error.column > 0);
    template_error_destroy(&error);
    arena_destroy(&arena);
}

static void test_rendering(const char *directory) {
    Arena arena;
    Context context = {0};
    TestViewRecord records[2] = {0};
    String output;

    assert(chdir(directory) == 0);
    assert(arena_init(&arena, 256));
    context.arena = &arena;
    records[0].name = string_from_in(&arena, sv("<first>"));
    records[0].score = 7;
    records[0].active = true;
    records[1].name = string_from_in(&arena, sv("second"));
    records[1].score = 8;
    assert(view_set(&context, sv("page_title"), view_string(sv("Records"))));
    assert(view_set(&context, sv("records"),
                    view_collection(records, 2, &test_view_definition)));
    output = string_new_in(&arena);
    assert(view_render_to_string(&context, sv("embedded"), &output));
    assert(stringv_equal(string_as_view(&output), sv("embedded")));
    write_file("views/embedded.html", "filesystem");
    string_clear(&output);
    assert(view_render_to_string(&context, sv("embedded"), &output));
    assert(stringv_equal(string_as_view(&output), sv("filesystem")));
    assert(unlink("views/embedded.html") == 0);
    string_clear(&output);
    assert(view_render_to_string(&context, sv("embedded"), &output));
    assert(stringv_equal(string_as_view(&output), sv("embedded")));
    string_clear(&output);
    assert(view_render_to_string(&context, sv("index"), &output));
    assert(stringv_equal(string_as_view(&output),
                         sv("&lt;first&gt;/Records;second/Records;")));

    write_file("views/index.html", sv("B").data);
    string_clear(&output);
    assert(view_render_to_string(&context, sv("index"), &output));
    assert(stringv_equal(string_as_view(&output), sv("B")));

    write_file("views/missing.html", "{{unknown}}");
    string_clear(&output);
    assert(!view_render_to_string(&context, sv("missing"), &output));
    assert(stringv_contains(view_last_error(&context), sv("unknown value")));
    assert(!view_render_to_string(&context, sv("../index"), &output));
    assert(stringv_equal(view_last_error(&context), sv("invalid view path")));

    arena_destroy(&arena);
}

static void test_layout(const char *directory) {
    Arena arena;
    Context context = {0};
    int sockets[2];
    char response[512] = {0};
    ssize_t received;

    assert(chdir(directory) == 0);
    assert(arena_init(&arena, 256));
    assert(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) == 0);
    context.arena = &arena;
    context.client_fd = sockets[0];
    assert(view_set(&context, sv("page_title"), view_string(sv("Layout"))));
    assert(view_set(&context, sv("records"),
                    view_collection(NULL, 0, &test_view_definition)));
    assert(render(&context, sv("index")));
    received = recv(sockets[1], response, sizeof(response) - 1, 0);
    assert(received > 0);
    response[received] = '\0';
    assert(strstr(response, "<title>Layout</title>B") != NULL);
    close(sockets[0]);
    close(sockets[1]);
    arena_destroy(&arena);
}

int main(void) {
    char original[4096];
    char temporary[] = "/tmp/ceasy-view-test-XXXXXX";
    char *directory;

    assert(getcwd(original, sizeof(original)) != NULL);
    directory = mkdtemp(temporary);
    assert(directory != NULL);
    assert(chdir(directory) == 0);
    setup_views();
    test_parser();
    test_rendering(directory);
    test_layout(directory);
    assert(chdir(directory) == 0);
    cleanup_views();
    assert(chdir(original) == 0);
    assert(rmdir(directory) == 0);
    return 0;
}
