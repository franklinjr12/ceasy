#include "ceasy/rendering/html.h"

bool html_escape_append(String *output, StringView value) {
    for (size_t index = 0; index < value.length; index++) {
        StringView escaped;

        switch (value.data[index]) {
        case '&':
            escaped = sv("&amp;");
            break;
        case '<':
            escaped = sv("&lt;");
            break;
        case '>':
            escaped = sv("&gt;");
            break;
        case '"':
            escaped = sv("&quot;");
            break;
        case '\'':
            escaped = sv("&#39;");
            break;
        default:
            if (!string_append_char(output, value.data[index])) {
                return false;
            }
            continue;
        }
        if (!string_append(output, escaped)) {
            return false;
        }
    }
    return true;
}
