#include "post.h"

#include <stddef.h>

static const ModelField post_fields[] = {
    {.name = sv("id"),
     .type = MODEL_FIELD_INT64,
     .offset = offsetof(Post, id),
     .primary_key = true,
     .insertable = false,
     .updatable = false},
    {.name = sv("title"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(Post, title),
     .primary_key = false,
     .insertable = true,
     .updatable = true},
    {.name = sv("content"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(Post, content),
     .primary_key = false,
     .insertable = true,
     .updatable = true},
    {.name = sv("created_at"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(Post, created_at),
     .primary_key = false,
     .insertable = false,
     .updatable = false},
    {.name = sv("updated_at"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(Post, updated_at),
     .primary_key = false,
     .insertable = false,
     .updatable = false},
};

static const ModelDefinition post_definition = {
    .name = sv("Post"),
    .table_name = sv("posts"),
    .size = sizeof(Post),
    .fields = post_fields,
    .field_count = sizeof(post_fields) / sizeof(post_fields[0]),
    .find_sql = sv("SELECT id, title, content, created_at, updated_at FROM "
                   "posts WHERE id = ?"),
    .all_sql = sv("SELECT id, title, content, created_at, updated_at FROM "
                  "posts ORDER BY id"),
    .insert_sql = sv("INSERT INTO posts (title, content) VALUES (?, ?)"),
    .update_sql = sv("UPDATE posts SET title = ?, content = ?, updated_at = "
                     "CURRENT_TIMESTAMP WHERE id = ?"),
    .delete_sql = sv("DELETE FROM posts WHERE id = ?"),
};

ModelResult post_find(Context *context, int64_t id, Post **post) {
    return model_find(context, &post_definition, id, (void **)post);
}
bool post_all(Context *context, PostArray *posts) {
    ModelArray result = {0};
    if (posts == NULL || !model_all(context, &post_definition, &result))
        return false;
    posts->items = (Post *)result.items;
    posts->length = result.length;
    return true;
}
bool post_insert(Context *context, Post *post) {
    return model_insert(context, &post_definition, post);
}
ModelResult post_update(Context *context, Post *post) {
    return model_update(context, &post_definition, post);
}
ModelResult post_destroy(Context *context, Post *post) {
    return model_destroy(context, &post_definition, post);
}
