#include "user.h"

#include <stddef.h>

static const ModelField user_fields[] = {
    {.name = sv("id"),
     .type = MODEL_FIELD_INT64,
     .offset = offsetof(User, id),
     .primary_key = true},
    {.name = sv("name"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(User, name),
     .insertable = true,
     .updatable = true},
    {.name = sv("email"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(User, email),
     .insertable = true,
     .updatable = true},
    {.name = sv("password_digest"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(User, password_digest),
     .insertable = true,
     .updatable = true},
    {.name = sv("bio"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(User, bio),
     .insertable = true,
     .updatable = true},
    {.name = sv("is_admin"),
     .type = MODEL_FIELD_BOOL,
     .offset = offsetof(User, is_admin),
     .insertable = true,
     .updatable = true},
    {.name = sv("created_at"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(User, created_at)},
    {.name = sv("updated_at"),
     .type = MODEL_FIELD_STRING,
     .offset = offsetof(User, updated_at)},
};

static const ModelDefinition user_definition = {
    .name = sv("User"),
    .table_name = sv("users"),
    .size = sizeof(User),
    .fields = user_fields,
    .field_count = sizeof(user_fields) / sizeof(user_fields[0]),
    .find_sql = sv("SELECT id, name, email, password_digest, bio, is_admin, "
                   "created_at, updated_at FROM users WHERE id = ?"),
    .all_sql = sv("SELECT id, name, email, password_digest, bio, is_admin, "
                  "created_at, updated_at FROM users ORDER BY id"),
    .insert_sql = sv("INSERT INTO users (name, email, password_digest, bio, "
                     "is_admin) VALUES (?, ?, ?, ?, ?)"),
    .update_sql =
        sv("UPDATE users SET name = ?, email = ?, password_digest = ?, "
           "bio = ?, is_admin = ?, updated_at = CURRENT_TIMESTAMP "
           "WHERE id = ?"),
    .delete_sql = sv("DELETE FROM users WHERE id = ?"),
};

ModelResult user_find(Context *context, int64_t id, User **user) {
    return model_find(context, &user_definition, id, (void **)user);
}

ModelResult user_find_by_email(Context *context, StringView email,
                               User **user) {
    DatabaseStatement statement = {0};
    DatabaseStepResult step;
    User *loaded;
    int64_t id;

    if (user != NULL)
        *user = NULL;
    if (user == NULL || context == NULL || context->arena == NULL ||
        !database_prepare(context->database, &statement,
                          sv("SELECT id, name, email, password_digest, bio, "
                             "is_admin, created_at, updated_at FROM users "
                             "WHERE email = ?")) ||
        !database_bind_text(&statement, 1, email)) {
        database_statement_destroy(&statement);
        return MODEL_RESULT_ERROR;
    }
    step = database_step(&statement);
    if (step == DATABASE_STEP_DONE) {
        database_statement_destroy(&statement);
        return MODEL_RESULT_NOT_FOUND;
    }
    if (step != DATABASE_STEP_ROW) {
        database_statement_destroy(&statement);
        return MODEL_RESULT_ERROR;
    }
    id = database_column_int64(&statement, 0);
    database_statement_destroy(&statement);
    return user_find(context, id, &loaded) == MODEL_RESULT_OK
               ? (*user = loaded, MODEL_RESULT_OK)
               : MODEL_RESULT_ERROR;
}

bool user_insert(Context *context, User *user) {
    return model_insert(context, &user_definition, user);
}
bool user_update(Context *context, User *user) {
    return model_update(context, &user_definition, user) == MODEL_RESULT_OK;
}
const ModelDefinition *user_model_definition(void) { return &user_definition; }
ViewValue user_view(const User *user) {
    return view_model(user, &user_definition);
}
