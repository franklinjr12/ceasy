#define _XOPEN_SOURCE 700

#include "../src/view_compiler.h"

#include <ceasy/ceasy.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern char *mkdtemp(char *template);

static void write_file(const char *path, const char *contents) {
    FILE *file = fopen(path, "wb");

    assert(file != NULL);
    assert(fwrite(contents, 1, strlen(contents), file) == strlen(contents));
    assert(fclose(file) == 0);
}

static String read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    long length;
    String result = string_new_heap();

    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    length = ftell(file);
    assert(length >= 0);
    assert(fseek(file, 0, SEEK_SET) == 0);
    assert(string_reserve(&result, (size_t)length));
    if (length > 0) {
        assert(fread(result.data, 1, (size_t)length, file) == (size_t)length);
    }
    result.length = (size_t)length;
    if (result.data != NULL) {
        result.data[result.length] = '\0';
    }
    assert(fclose(file) == 0);
    return result;
}

static void remove_tree(void) {
    assert(unlink("src/generated/ceasy_views.c") == 0);
    unlink("src/generated/ceasy_views.c.tmp");
    assert(rmdir("src/generated") == 0);
    unlink("views/page.html");
    unlink("views/posts/_card.html");
    unlink("views/posts/_a.html");
    unlink("views/posts/_b.html");
    assert(rmdir("views/posts") == 0);
    assert(rmdir("views") == 0);
    unlink("bundle.o");
    assert(rmdir("src") == 0);
}

int main(void) {
    char original[PATH_MAX];
    char template[] = "/tmp/ceasy-view-compiler-XXXXXX";
    char *root = mkdtemp(template);
    CeasyProject project = {0};
    String first;
    String second;
    char command[PATH_MAX * 2];

    assert(getcwd(original, sizeof(original)) != NULL);
    assert(root != NULL);
    strcpy(project.root, root);
    assert(chdir(root) == 0);
    assert(mkdir("views", 0755) == 0);
    assert(mkdir("views/posts", 0755) == 0);
    assert(mkdir("src", 0755) == 0);
    write_file("views/page.html",
               "quotes \" slash \\\\ \t\n UTF-8 \303\251 {{> posts/card}}\n");
    write_file("views/posts/_card.html", "card");
    assert(view_compiler_run(&project));
    first = read_file("src/generated/ceasy_views.c");
    assert(stringv_contains(string_as_view(&first),
                            sv("TEMPLATE_INSTRUCTION_PARTIAL")));
    assert(snprintf(command, sizeof(command),
                    "clang -std=c17 -I/workspace/include -c "
                    "%s/src/generated/ceasy_views.c -o %s/bundle.o",
                    root, root) > 0);
    assert(system(command) == 0);
    assert(view_compiler_run(&project));
    second = read_file("src/generated/ceasy_views.c");
    assert(stringv_equal(string_as_view(&first), string_as_view(&second)));

    write_file("views/page.html", "{{> missing}}");
    assert(!view_compiler_run(&project));
    string_destroy(&second);
    second = read_file("src/generated/ceasy_views.c");
    assert(stringv_equal(string_as_view(&first), string_as_view(&second)));

    write_file("views/page.html", "{{> posts/a}}");
    write_file("views/posts/_a.html", "{{> posts/b}}");
    write_file("views/posts/_b.html", "{{> posts/a}}");
    assert(!view_compiler_run(&project));

    string_destroy(&first);
    string_destroy(&second);
    remove_tree();
    assert(chdir(original) == 0);
    assert(rmdir(root) == 0);
    return 0;
}
