#include "home_controller.h"

#include "posts_controller.h"

void home_index(Context *context) { posts_index(context); }
