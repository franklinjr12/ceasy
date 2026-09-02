#ifndef POSTS_CONTROLLER_H
#define POSTS_CONTROLLER_H

#include <ceasy/ceasy.h>

void posts_index(Context *context);
void posts_show(Context *context);
void posts_new(Context *context);
void posts_create(Context *context);
void posts_edit(Context *context);
void posts_update(Context *context);
void posts_destroy(Context *context);

#endif
