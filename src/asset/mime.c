#include "ceasy/asset/asset.h"

#include <string.h>

StringView asset_content_type(StringView path) {
    size_t slash = 0;
    size_t dot;
    StringView extension;

    if (path.length > 0 && path.data == NULL) {
        return sv("application/octet-stream");
    }
    for (size_t index = path.length; index > 0; index--) {
        if (path.data[index - 1] == '/') {
            slash = index;
            break;
        }
    }
    dot = path.length;
    for (size_t index = path.length; index > slash; index--) {
        if (path.data[index - 1] == '.') {
            dot = index - 1;
            break;
        }
    }
    if (dot == path.length || dot + 1 >= path.length) {
        return sv("application/octet-stream");
    }
    extension = stringv_from(path, dot);
    if (stringv_equal_ignore_case(extension, sv(".css"))) {
        return sv("text/css; charset=utf-8");
    }
    if (stringv_equal_ignore_case(extension, sv(".js"))) {
        return sv("text/javascript; charset=utf-8");
    }
    if (stringv_equal_ignore_case(extension, sv(".html"))) {
        return sv("text/html; charset=utf-8");
    }
    if (stringv_equal_ignore_case(extension, sv(".txt"))) {
        return sv("text/plain; charset=utf-8");
    }
    if (stringv_equal_ignore_case(extension, sv(".json"))) {
        return sv("application/json; charset=utf-8");
    }
    if (stringv_equal_ignore_case(extension, sv(".svg"))) {
        return sv("image/svg+xml");
    }
    if (stringv_equal_ignore_case(extension, sv(".png"))) {
        return sv("image/png");
    }
    if (stringv_equal_ignore_case(extension, sv(".jpg")) ||
        stringv_equal_ignore_case(extension, sv(".jpeg"))) {
        return sv("image/jpeg");
    }
    if (stringv_equal_ignore_case(extension, sv(".gif"))) {
        return sv("image/gif");
    }
    if (stringv_equal_ignore_case(extension, sv(".webp"))) {
        return sv("image/webp");
    }
    if (stringv_equal_ignore_case(extension, sv(".ico"))) {
        return sv("image/x-icon");
    }
    if (stringv_equal_ignore_case(extension, sv(".woff"))) {
        return sv("font/woff");
    }
    if (stringv_equal_ignore_case(extension, sv(".woff2"))) {
        return sv("font/woff2");
    }
    return sv("application/octet-stream");
}
