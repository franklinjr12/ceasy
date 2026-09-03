#include "dashboard_controller.h"

#include "../models/post.h"
#include "concerns/blog_controller.h"

void dashboard_index(Context *context) {
    User *user = blog_current_user(context);
    PostArray posts = {0};
    if (user == NULL || !post_all_for_user(context, user->id, &posts) ||
        !view_set(context, sv("page_title"), view_string(sv("Dashboard"))) ||
        !view_set(context, sv("posts"), post_array_view(posts))) {
        blog_error(context, sv("500 Internal Server Error"),
                   sv("Something went wrong.\n"));
        return;
    }
    blog_render(context, sv("dashboard/index"), sv("200 OK"));
}
