#ifndef POST_MODEL_H
#define POST_MODEL_H

#include <ceasy/ceasy.h>

typedef struct {
    int64_t id;
    int64_t user_id;
    String title;
    String summary;
    String content;
    bool published;
    String published_at;
    String created_at;
    String updated_at;
} Post;

typedef struct {
    Post *items;
    size_t length;
} PostArray;

ModelResult post_find(Context *context, int64_t id, Post **post);
bool post_all(Context *context, PostArray *posts);
bool post_all_for_user(Context *context, int64_t user_id, PostArray *posts);
bool post_all_published_for_user(Context *context, int64_t user_id,
                                 PostArray *posts);
bool post_insert(Context *context, Post *post);
ModelResult post_update(Context *context, Post *post);
ModelResult post_destroy(Context *context, Post *post);
const ModelDefinition *post_model_definition(void);
ViewValue post_view(const Post *post);
ViewValue post_array_view(PostArray posts);

#endif
