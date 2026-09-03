#include "blog_queries.h"

#include <stddef.h>
#include <string.h>

static const ModelField card_fields[] = {
    {.name = sv("id"),
     .type = MODEL_FIELD_INT64,
     .offset = offsetof(PostCard, id)},
    {.name = sv("title"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(PostCard, title)},
    {.name = sv("summary"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(PostCard, summary)},
    {.name = sv("author_id"),
     .type = MODEL_FIELD_INT64,
     .offset = offsetof(PostCard, author_id)},
    {.name = sv("author_name"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(PostCard, author_name)},
    {.name = sv("published_at"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(PostCard, published_at)},
    {.name = sv("comment_count"),
     .type = MODEL_FIELD_INT64,
     .offset = offsetof(PostCard, comment_count)},
};
static const ModelDefinition card_definition = {
    .name = sv("PostCard"),
    .size = sizeof(PostCard),
    .fields = card_fields,
    .field_count = sizeof(card_fields) / sizeof(card_fields[0])};
static const ModelField comment_fields[] = {
    {.name = sv("id"),
     .type = MODEL_FIELD_INT64,
     .offset = offsetof(CommentView, id)},
    {.name = sv("user_id"),
     .type = MODEL_FIELD_INT64,
     .offset = offsetof(CommentView, user_id)},
    {.name = sv("author_name"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(CommentView, author_name)},
    {.name = sv("content"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(CommentView, content)},
    {.name = sv("created_at"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(CommentView, created_at)},
    {.name = sv("can_delete"),
     .type = MODEL_FIELD_BOOL,
     .offset = offsetof(CommentView, can_delete)},
};
static const ModelDefinition comment_definition = {
    .name = sv("CommentView"),
    .size = sizeof(CommentView),
    .fields = comment_fields,
    .field_count = sizeof(comment_fields) / sizeof(comment_fields[0])};

static String query_copy(Context *context, DatabaseStatement *statement,
                         int column) {
    return string_from_in(context->arena,
                          database_column_text(statement, column));
}

bool post_cards_query(Context *context, StringView search, int64_t page,
                      PostCardArray *cards, bool *has_next) {
    DatabaseStatement statement = {0};
    DatabaseStepResult step;
    PostCard *items = NULL;
    size_t length = 0;
    int64_t offset;
    String pattern = {0};
    bool filtered = search.length > 0;
    const char *sql =
        filtered ? "SELECT "
                   "p.id,p.title,p.summary,u.id,u.name,p.published_at,(SELECT "
                   "COUNT(*) FROM comments c WHERE c.post_id=p.id) FROM posts "
                   "p JOIN users u ON u.id=p.user_id WHERE p.published=1 AND "
                   "(p.title LIKE ? OR p.summary LIKE ? OR p.content LIKE ?) "
                   "ORDER BY p.published_at DESC,p.id DESC LIMIT 11 OFFSET ?"
                 : "SELECT "
                   "p.id,p.title,p.summary,u.id,u.name,p.published_at,(SELECT "
                   "COUNT(*) FROM comments c WHERE c.post_id=p.id) FROM posts "
                   "p JOIN users u ON u.id=p.user_id WHERE p.published=1 ORDER "
                   "BY p.published_at DESC,p.id DESC LIMIT 11 OFFSET ?";
    if (cards == NULL || has_next == NULL || context == NULL || page < 1 ||
        page > INT64_MAX / 10 ||
        !database_prepare(context->database, &statement,
                          stringv_from_cstr(sql))) {
        database_statement_destroy(&statement);
        return false;
    }
    offset = (page - 1) * 10;
    if (filtered) {
        pattern = string_new_in(context->arena);
        if (!string_append_char(&pattern, '%') ||
            !string_append(&pattern, search) ||
            !string_append_char(&pattern, '%') ||
            !database_bind_text(&statement, 1, string_as_view(&pattern)) ||
            !database_bind_text(&statement, 2, string_as_view(&pattern)) ||
            !database_bind_text(&statement, 3, string_as_view(&pattern)) ||
            !database_bind_int64(&statement, 4, offset)) {
            database_statement_destroy(&statement);
            return false;
        }
    } else if (!database_bind_int64(&statement, 1, offset)) {
        database_statement_destroy(&statement);
        return false;
    }
    while ((step = database_step(&statement)) == DATABASE_STEP_ROW) {
        PostCard *replacement;
        if (length == 11)
            break;
        replacement =
            arena_alloc(context->arena, (length + 1) * sizeof(*replacement));
        if (replacement == NULL) {
            database_statement_destroy(&statement);
            return false;
        }
        if (items != NULL)
            memcpy(replacement, items, length * sizeof(*items));
        items = replacement;
        memset(&items[length], 0, sizeof(items[length]));
        items[length].id = database_column_int64(&statement, 0);
        items[length].title = query_copy(context, &statement, 1);
        items[length].summary = query_copy(context, &statement, 2);
        items[length].author_id = database_column_int64(&statement, 3);
        items[length].author_name = query_copy(context, &statement, 4);
        items[length].published_at = query_copy(context, &statement, 5);
        items[length].comment_count = database_column_int64(&statement, 6);
        if (items[length].title.data == NULL ||
            items[length].summary.data == NULL ||
            items[length].author_name.data == NULL ||
            items[length].published_at.data == NULL) {
            database_statement_destroy(&statement);
            return false;
        }
        length++;
    }
    database_statement_destroy(&statement);
    if (step == DATABASE_STEP_ERROR)
        return false;
    *has_next = length > 10;
    cards->items = items;
    cards->length = *has_next ? 10 : length;
    return true;
}

bool comments_query(Context *context, int64_t post_id, int64_t viewer_id,
                    bool viewer_admin, CommentViewArray *comments) {
    DatabaseStatement statement = {0};
    CommentView *items = NULL;
    size_t length = 0;
    DatabaseStepResult step;
    if (comments == NULL || context == NULL ||
        !database_prepare(
            context->database, &statement,
            sv("SELECT c.id,c.user_id,u.name,c.content,c.created_at FROM "
               "comments c JOIN users u ON u.id=c.user_id WHERE c.post_id=? "
               "ORDER BY c.created_at,c.id")) ||
        !database_bind_int64(&statement, 1, post_id)) {
        database_statement_destroy(&statement);
        return false;
    }
    while ((step = database_step(&statement)) == DATABASE_STEP_ROW) {
        CommentView *replacement =
            arena_alloc(context->arena, (length + 1) * sizeof(*replacement));
        if (replacement == NULL) {
            database_statement_destroy(&statement);
            return false;
        }
        if (items != NULL)
            memcpy(replacement, items, length * sizeof(*items));
        items = replacement;
        memset(&items[length], 0, sizeof(items[length]));
        items[length].id = database_column_int64(&statement, 0);
        items[length].user_id = database_column_int64(&statement, 1);
        items[length].author_name = query_copy(context, &statement, 2);
        items[length].content = query_copy(context, &statement, 3);
        items[length].created_at = query_copy(context, &statement, 4);
        items[length].can_delete =
            viewer_admin || viewer_id == items[length].user_id;
        if (items[length].author_name.data == NULL ||
            items[length].content.data == NULL) {
            database_statement_destroy(&statement);
            return false;
        }
        length++;
    }
    database_statement_destroy(&statement);
    if (step == DATABASE_STEP_ERROR)
        return false;
    comments->items = items;
    comments->length = length;
    return true;
}
ViewValue post_card_array_view(PostCardArray cards) {
    return view_collection(cards.items, cards.length, &card_definition);
}
ViewValue comment_array_view(CommentViewArray comments) {
    return view_collection(comments.items, comments.length,
                           &comment_definition);
}
