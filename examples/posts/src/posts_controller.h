#ifndef POSTS_CONTROLLER_H
#define POSTS_CONTROLLER_H

#include <ceasy/ceasy.h>

#include "models/post.h"

void posts_index(Context *context);
void posts_show(Context *context);
void posts_new(Context *context);
void posts_create(Context *context);
void posts_edit(Context *context);
void posts_update(Context *context);
void posts_destroy(Context *context);
bool require_authenticated_user(Context *context);
void home_index(Context *context);
void users_new(Context *context);
void users_create(Context *context);
void sessions_new(Context *context);
void sessions_create(Context *context);
void sessions_destroy(Context *context);
void dashboard_index(Context *context);
void comments_create(Context *context);
void comments_destroy(Context *context);
void authors_show(Context *context);
void account_edit(Context *context);
void account_update(Context *context);
void account_password_update(Context *context);
void health_index(Context *context);
void ready_index(Context *context);

#endif
