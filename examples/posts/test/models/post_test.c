#include "../../src/models/post.h"
#include <assert.h>

int main(void) {
    Post post = {0};
    assert(post.id == 0);
    assert(post.title.data == NULL);
    return 0;
}
