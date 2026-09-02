#ifndef CEASY_RENDERING_HTML_H
#define CEASY_RENDERING_HTML_H

#include <stdbool.h>

#include <ceasy/string/string.h>

bool html_escape_append(String *output, StringView value);

#endif
