#include "comment.h"

#include <stddef.h>

static const ModelField comment_fields[] = {
    {.name = sv("id"),
     .type = MODEL_FIELD_INT64,
     .offset = offsetof(Comment, id),
     .primary_key = true},
    {.name = sv("post_id"),
     .type = MODEL_FIELD_INT64,
     .offset = offsetof(Comment, post_id),
     .insertable = true},
    {.name = sv("user_id"),
     .type = MODEL_FIELD_INT64,
     .offset = offsetof(Comment, user_id),
     .insertable = true},
    {.name = sv("content"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(Comment, content),
     .insertable = true,
     .updatable = true},
    {.name = sv("created_at"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(Comment, created_at)},
    {.name = sv("updated_at"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(Comment, updated_at)},
};
static const ModelDefinition comment_definition = {
    .name = sv("Comment"),
    .table_name = sv("comments"),
    .size = sizeof(Comment),
    .fields = comment_fields,
    .field_count = sizeof(comment_fields) / sizeof(comment_fields[0]),
    .find_sql = sv("SELECT id, post_id, user_id, content, created_at, "
                   "updated_at FROM comments WHERE id = ?"),
    .all_sql = sv("SELECT id, post_id, user_id, content, created_at, "
                  "updated_at FROM comments ORDER BY id"),
    .insert_sql =
        sv("INSERT INTO comments (post_id, user_id, content) VALUES (?, ?, ?)"),
    .update_sql = sv("UPDATE comments SET content = ?, updated_at = "
                     "CURRENT_TIMESTAMP WHERE id = ?"),
    .delete_sql = sv("DELETE FROM comments WHERE id = ?"),
};
bool comment_insert(Context *context, Comment *comment) {
    return model_insert(context, &comment_definition, comment);
}
const ModelDefinition *comment_model_definition(void) {
    return &comment_definition;
}
