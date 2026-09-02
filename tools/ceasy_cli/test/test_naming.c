#include "naming.h"

#include <assert.h>
#include <string.h>

int main(void) {
    Naming naming;
    FieldSpec fields[CEASY_FIELD_CAPACITY];
    size_t count;
    char error[256];
    char *valid[] = {"title:string", "content:text", "views:int64",
                     "published:bool", "rank:integer"};
    char *invalid_type[] = {"price:decimal"};
    char *duplicate[] = {"title:string", "title:text"};
    char *reserved[] = {"id:int64"};

    assert(naming_model("Post", &naming, error, sizeof(error)));
    assert(strcmp(naming.singular, "post") == 0);
    assert(strcmp(naming.plural, "posts") == 0);
    assert(naming_model("BlogPost", &naming, error, sizeof(error)));
    assert(strcmp(naming.singular, "blog_post") == 0);
    assert(strcmp(naming.plural, "blog_posts") == 0);
    assert(naming_parse_fields(5, valid, fields, &count, error, sizeof(error)));
    assert(count == 5);
    assert(fields[0].type == MODEL_FIELD_STRING);
    assert(fields[1].type == MODEL_FIELD_STRING);
    assert(fields[2].type == MODEL_FIELD_INT64);
    assert(fields[3].type == MODEL_FIELD_BOOL);
    assert(fields[4].type == MODEL_FIELD_INT64);
    assert(!naming_parse_fields(1, invalid_type, fields, &count, error,
                                sizeof(error)));
    assert(!naming_parse_fields(2, duplicate, fields, &count, error,
                                sizeof(error)));
    assert(!naming_parse_fields(1, reserved, fields, &count, error,
                                sizeof(error)));
    assert(!naming_model("posts", &naming, error, sizeof(error)));
    return 0;
}
