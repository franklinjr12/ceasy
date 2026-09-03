#ifndef BLOG_QUERIES_H
#define BLOG_QUERIES_H

#include <ceasy/ceasy.h>

typedef struct {
    int64_t id;
    String title;
    String summary;
    int64_t author_id;
    String author_name;
    String published_at;
    int64_t comment_count;
} PostCard;
typedef struct {
    PostCard *items;
    size_t length;
} PostCardArray;
typedef struct {
    int64_t id;
    int64_t user_id;
    String author_name;
    String content;
    String created_at;
    bool can_delete;
} CommentView;
typedef struct {
    CommentView *items;
    size_t length;
} CommentViewArray;

bool post_cards_query(Context *context, StringView search, int64_t page,
                      PostCardArray *cards, bool *has_next);
bool comments_query(Context *context, int64_t post_id, int64_t viewer_id,
                    bool viewer_admin, CommentViewArray *comments);
ViewValue post_card_array_view(PostCardArray cards);
ViewValue comment_array_view(CommentViewArray comments);

#endif
