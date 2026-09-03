#ifndef BLOG_USER_MODEL_H
#define BLOG_USER_MODEL_H

#include <ceasy/ceasy.h>

typedef struct {
    int64_t id;
    String name;
    String email;
    String password_digest;
    String bio;
    bool is_admin;
    String created_at;
    String updated_at;
} User;

typedef struct {
    User *items;
    size_t length;
} UserArray;

ModelResult user_find(Context *context, int64_t id, User **user);
ModelResult user_find_by_email(Context *context, StringView email, User **user);
bool user_insert(Context *context, User *user);
bool user_update(Context *context, User *user);
const ModelDefinition *user_model_definition(void);
ViewValue user_view(const User *user);

#endif
