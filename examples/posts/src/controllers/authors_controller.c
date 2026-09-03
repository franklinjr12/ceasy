#include "authors_controller.h"

#include "../models/post.h"
#include "../models/user.h"
#include "concerns/blog_controller.h"

void authors_show(Context *context) {
    int64_t id;
    User *user = NULL;
    PostArray posts = {0};
    if (!blog_id(context, &id) ||
        user_find(context, id, &user) != MODEL_RESULT_OK ||
        !post_all_published_for_user(context, id, &posts)) {
        blog_error(context, sv("404 Not Found"),
                   sv("We couldn't find that page.\n"));
        return;
    }
    view_set(context, sv("page_title"),
             view_string(string_as_view(&user->name)));
    view_set(context, sv("author"), user_view(user));
    view_set(context, sv("posts"), post_array_view(posts));
    blog_render(context, sv("authors/show"), sv("200 OK"));
}
