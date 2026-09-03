#ifndef BLOG_COMMENT_MODEL_H
#define BLOG_COMMENT_MODEL_H

#include <ceasy/ceasy.h>

typedef struct {
    int64_t id;
    int64_t post_id;
    int64_t user_id;
    String content;
    String created_at;
    String updated_at;
} Comment;

typedef struct {
    Comment *items;
    size_t length;
} CommentArray;

bool comment_insert(Context *context, Comment *comment);
const ModelDefinition *comment_model_definition(void);

#endif
