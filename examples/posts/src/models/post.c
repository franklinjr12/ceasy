#include "post.h"

#include <stddef.h>
#include <string.h>

static const ModelField post_fields[] = {
    {.name = sv("id"),
     .type = MODEL_FIELD_INT64,
     .offset = offsetof(Post, id),
     .primary_key = true,
     .insertable = false,
     .updatable = false},
    {.name = sv("user_id"),
     .type = MODEL_FIELD_INT64,
     .offset = offsetof(Post, user_id),
     .insertable = true,
     .updatable = false},
    {.name = sv("title"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(Post, title),
     .primary_key = false,
     .insertable = true,
     .updatable = true},
    {.name = sv("summary"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(Post, summary),
     .insertable = true,
     .updatable = true},
    {.name = sv("content"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(Post, content),
     .primary_key = false,
     .insertable = true,
     .updatable = true},
    {.name = sv("published"),
     .type = MODEL_FIELD_BOOL,
     .offset = offsetof(Post, published),
     .insertable = true,
     .updatable = true},
    {.name = sv("published_at"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(Post, published_at),
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
    .find_sql = sv("SELECT id, user_id, title, summary, content, published, "
                   "published_at, created_at, updated_at FROM "
                   "posts WHERE id = ?"),
    .all_sql = sv("SELECT id, user_id, title, summary, content, published, "
                  "published_at, created_at, updated_at FROM "
                  "posts ORDER BY id"),
    .insert_sql = sv("INSERT INTO posts (user_id, title, summary, content, "
                     "published, published_at) VALUES (?, ?, ?, ?, ?, ?)"),
    .update_sql = sv("UPDATE posts SET title = ?, summary = ?, content = ?, "
                     "published = ?, published_at = ?, updated_at = "
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
static bool post_all_where_user(Context *context, int64_t user_id,
                                bool published_only, PostArray *posts) {
    DatabaseStatement statement = {0};
    DatabaseStepResult step;
    Post *items = NULL;
    size_t length = 0;
    size_t capacity = 0;

    if (posts == NULL || context == NULL || context->arena == NULL ||
        !database_prepare(
            context->database, &statement,
            sv("SELECT id, user_id, title, summary, content, "
               "published, published_at, created_at, updated_at "
               "FROM posts WHERE user_id = ? AND (? = 0 OR published = 1) "
               "ORDER BY id DESC")) ||
        !database_bind_int64(&statement, 1, user_id)) {
        database_statement_destroy(&statement);
        return false;
    }
    if (!database_bind_int64(&statement, 2, published_only ? 1 : 0)) {
        database_statement_destroy(&statement);
        return false;
    }
    while ((step = database_step(&statement)) == DATABASE_STEP_ROW) {
        if (length == capacity) {
            size_t next_capacity = capacity == 0 ? 8 : capacity * 2;
            Post *replacement = arena_alloc(
                context->arena, next_capacity * sizeof(*replacement));
            if (replacement == NULL) {
                database_statement_destroy(&statement);
                return false;
            }
            if (items != NULL)
                memcpy(replacement, items, length * sizeof(*items));
            items = replacement;
            capacity = next_capacity;
        }
        memset(&items[length], 0, sizeof(items[length]));
        items[length].id = database_column_int64(&statement, 0);
        items[length].user_id = database_column_int64(&statement, 1);
        items[length].title =
            string_from_in(context->arena, database_column_text(&statement, 2));
        items[length].summary =
            string_from_in(context->arena, database_column_text(&statement, 3));
        items[length].content =
            string_from_in(context->arena, database_column_text(&statement, 4));
        items[length].published = database_column_int64(&statement, 5) != 0;
        items[length].published_at =
            string_from_in(context->arena, database_column_text(&statement, 6));
        items[length].created_at =
            string_from_in(context->arena, database_column_text(&statement, 7));
        items[length].updated_at =
            string_from_in(context->arena, database_column_text(&statement, 8));
        if (items[length].title.data == NULL ||
            items[length].summary.data == NULL ||
            items[length].content.data == NULL ||
            items[length].published_at.data == NULL ||
            items[length].created_at.data == NULL ||
            items[length].updated_at.data == NULL) {
            database_statement_destroy(&statement);
            return false;
        }
        length++;
    }
    database_statement_destroy(&statement);
    if (step == DATABASE_STEP_ERROR)
        return false;
    posts->items = items;
    posts->length = length;
    return true;
}
bool post_all_for_user(Context *context, int64_t user_id, PostArray *posts) {
    return post_all_where_user(context, user_id, false, posts);
}
bool post_all_published_for_user(Context *context, int64_t user_id,
                                 PostArray *posts) {
    return post_all_where_user(context, user_id, true, posts);
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
const ModelDefinition *post_model_definition(void) { return &post_definition; }
ViewValue post_view(const Post *post) {
    return view_model(post, &post_definition);
}
ViewValue post_array_view(PostArray posts) {
    return view_collection(posts.items, posts.length, &post_definition);
}
