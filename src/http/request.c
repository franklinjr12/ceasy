#include "ceasy/http/request.h"

#include <string.h>

static bool request_find_bytes(StringView value, const char *bytes,
                               size_t length, size_t *index) {
    if (length == 0 || value.length < length) {
        return false;
    }

    for (size_t position = 0; position <= value.length - length; position++) {
        if (memcmp(value.data + position, bytes, length) == 0) {
            if (index != NULL) {
                *index = position;
            }
            return true;
        }
    }
    return false;
}

static bool request_has_header(const Request *request, StringView name) {
    if (request == NULL) {
        return false;
    }
    for (size_t index = 0; index < request->header_count; index++) {
        if (stringv_equal_ignore_case(request->headers[index].name, name)) {
            return true;
        }
    }
    return false;
}

static bool request_transfer_is_chunked(StringView value) {
    StringView remaining = value;

    while (remaining.length > 0) {
        StringView token;
        StringView next;

        if (stringv_split_once_char(remaining, ',', &token, &next)) {
            remaining = next;
        } else {
            token = remaining;
            remaining = (StringView){0};
        }
        if (stringv_equal_ignore_case(stringv_trim(token), sv("chunked"))) {
            return true;
        }
    }
    return false;
}

HttpMethod http_method_parse_view(StringView method) {
    if (stringv_equal(method, sv("GET"))) {
        return HTTP_METHOD_GET;
    }
    if (stringv_equal(method, sv("POST"))) {
        return HTTP_METHOD_POST;
    }
    if (stringv_equal(method, sv("PATCH"))) {
        return HTTP_METHOD_PATCH;
    }
    if (stringv_equal(method, sv("PUT"))) {
        return HTTP_METHOD_PUT;
    }
    if (stringv_equal(method, sv("DELETE"))) {
        return HTTP_METHOD_DELETE;
    }
    return HTTP_METHOD_UNKNOWN;
}

HttpMethod http_method_parse(const char *method) {
    return http_method_parse_view(stringv_from_cstr(method));
}

HttpMethod http_method_parse_cstr(const char *method) {
    return http_method_parse(method);
}

StringView request_header(const Request *request, StringView name) {
    if (request == NULL) {
        return (StringView){0};
    }

    for (size_t index = 0; index < request->header_count; index++) {
        if (stringv_equal_ignore_case(request->headers[index].name, name)) {
            return request->headers[index].value;
        }
    }
    return (StringView){0};
}

static RequestParseResult
request_parse_headers(Request *request, StringView raw, size_t header_end) {
    size_t line_end;
    StringView line;
    StringView method;
    StringView target;
    StringView version;
    StringView request_line_rest;
    StringView remaining;

    if (!stringv_find(raw, sv("\r\n"), &line_end)) {
        return REQUEST_PARSE_INCOMPLETE;
    }
    line = stringv_slice(raw, 0, line_end);
    if (!stringv_split_once_char(line, ' ', &method, &request_line_rest) ||
        !stringv_split_once_char(request_line_rest, ' ', &target, &version) ||
        !stringv_equal(version, sv("HTTP/1.1")) || target.length == 0 ||
        target.data[0] != '/') {
        return REQUEST_PARSE_BAD_REQUEST;
    }

    remaining = stringv_from(raw, line_end + 2);

    request->method = http_method_parse_view(method);
    request->path = target;
    size_t query_index;
    if (stringv_find_char(request->path, '?', &query_index)) {
        request->query_string = stringv_from(request->path, query_index + 1);
        request->path = stringv_slice(request->path, 0, query_index);
    }
    if (request->path.length == 0) {
        return REQUEST_PARSE_BAD_REQUEST;
    }

    while (remaining.length > 0 &&
           (size_t)(remaining.data - raw.data) < header_end) {
        size_t relative_end;
        size_t absolute_end;
        StringView name;
        StringView value;

        if (!stringv_find(remaining, sv("\r\n"), &relative_end)) {
            return REQUEST_PARSE_BAD_REQUEST;
        }
        absolute_end = (size_t)(remaining.data - raw.data) + relative_end;
        if (absolute_end > header_end) {
            return REQUEST_PARSE_BAD_REQUEST;
        }
        line = stringv_slice(remaining, 0, relative_end);
        remaining = stringv_from(remaining, relative_end + 2);
        if (line.length == 0) {
            break;
        }
        if (request->header_count >= CEASY_MAX_HEADERS ||
            !stringv_split_once_char(line, ':', &name, &value)) {
            return REQUEST_PARSE_BAD_REQUEST;
        }
        name = stringv_trim(name);
        value = stringv_trim(value);
        if (name.length == 0) {
            return REQUEST_PARSE_BAD_REQUEST;
        }
        request->headers[request->header_count++] =
            (RequestHeader){.name = name, .value = value};
    }

    request->content_type = request_header(request, sv("Content-Type"));
    StringView transfer_encoding =
        request_header(request, sv("Transfer-Encoding"));
    if (request_transfer_is_chunked(transfer_encoding)) {
        return REQUEST_PARSE_UNSUPPORTED;
    }

    StringView content_length = request_header(request, sv("Content-Length"));
    if (request_has_header(request, sv("Content-Length"))) {
        if (!stringv_parse_size(content_length, &request->content_length)) {
            return REQUEST_PARSE_BAD_REQUEST;
        }
        if (request->content_length > CEASY_MAX_REQUEST_BODY) {
            return REQUEST_PARSE_PAYLOAD_TOO_LARGE;
        }
    }
    return REQUEST_PARSE_OK;
}

RequestParseResult request_parse(Request *request, StringView raw) {
    size_t header_end;
    RequestParseResult result;

    if (request == NULL || raw.data == NULL) {
        return REQUEST_PARSE_BAD_REQUEST;
    }
    memset(request, 0, sizeof(*request));
    if (!request_find_bytes(raw, "\r\n\r\n", 4, &header_end)) {
        return raw.length >= CEASY_MAX_HEADER_BYTES ? REQUEST_PARSE_BAD_REQUEST
                                                    : REQUEST_PARSE_INCOMPLETE;
    }
    if (header_end + 4 > CEASY_MAX_HEADER_BYTES) {
        return REQUEST_PARSE_BAD_REQUEST;
    }

    result = request_parse_headers(request, raw, header_end);
    if (result != REQUEST_PARSE_OK) {
        return result;
    }
    if (raw.length < header_end + 4 + request->content_length) {
        return REQUEST_PARSE_INCOMPLETE;
    }
    request->body = stringv_slice(raw, header_end + 4, request->content_length);
    return REQUEST_PARSE_OK;
}
