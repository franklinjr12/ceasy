#include "ceasy/rendering/html.h"

#include <assert.h>

int main(void) {
    String html = string_new_heap();

    assert(html_escape_append(&html, sv("&<>\"' plain UTF-8")));
    assert(stringv_equal(string_as_view(&html),
                         sv("&amp;&lt;&gt;&quot;&#39; plain UTF-8")));
    string_destroy(&html);
    return 0;
}
