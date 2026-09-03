#ifndef BLOG_CONTROLLER_CONCERN_H
#define BLOG_CONTROLLER_CONCERN_H

#include <ceasy/ceasy.h>
#include <ceasy/validation/validation.h>

#include "../../models/post.h"
#include "../../models/user.h"

void blog_error(Context *context, StringView status, StringView message);
bool blog_id(Context *context, int64_t *id);
User *blog_current_user(Context *context);
bool require_authenticated_user(Context *context);
bool blog_render(Context *context, StringView path, StringView status);
String normalized(Context *context, StringView value, bool lower);
void form_errors(Context *context, ValidationErrors *errors);
bool blog_render_post_show(Context *context, Post *post, User *viewer,
                           StringView status);

#endif
