#ifndef POST_MODEL_H
#define POST_MODEL_H

#include <ceasy/ceasy.h>

typedef struct {
    int64_t id;
    String title;
    String content;
    String created_at;
    String updated_at;
} Post;

typedef struct {
    Post *items;
    size_t length;
} PostArray;

ModelResult post_find(Context *context, int64_t id, Post **post);
bool post_all(Context *context, PostArray *posts);
bool post_insert(Context *context, Post *post);
ModelResult post_update(Context *context, Post *post);
ModelResult post_destroy(Context *context, Post *post);

#endif
