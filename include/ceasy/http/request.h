#ifndef CEASY_HTTP_REQUEST_H
#define CEASY_HTTP_REQUEST_H

#include <stdbool.h>
#include <stddef.h>

#include <ceasy/string/string.h>

#define CEASY_MAX_HEADER_BYTES (16 * 1024)
#define CEASY_MAX_REQUEST_BODY (1024 * 1024)
#define CEASY_MAX_HEADERS 64

typedef enum {
    HTTP_METHOD_UNKNOWN = 0,
    HTTP_METHOD_GET,
    HTTP_METHOD_POST,
    HTTP_METHOD_PATCH,
    HTTP_METHOD_PUT,
    HTTP_METHOD_DELETE
} HttpMethod;

typedef struct {
    StringView name;
    StringView value;
} RequestHeader;

typedef struct {
    HttpMethod method;
    StringView path;
    StringView query_string;
    StringView body;
    StringView content_type;
    size_t content_length;
    RequestHeader headers[CEASY_MAX_HEADERS];
    size_t header_count;
} Request;

typedef enum {
    REQUEST_PARSE_OK = 0,
    REQUEST_PARSE_INCOMPLETE,
    REQUEST_PARSE_BAD_REQUEST,
    REQUEST_PARSE_PAYLOAD_TOO_LARGE,
    REQUEST_PARSE_UNSUPPORTED
} RequestParseResult;

HttpMethod http_method_parse(const char *method);
HttpMethod http_method_parse_view(StringView method);
HttpMethod http_method_parse_cstr(const char *method);
/* Returned header views borrow raw request storage. */
StringView request_header(const Request *request, StringView name);
/* Raw request storage is borrowed by parsed fields; caller owns its lifetime.
 */
RequestParseResult request_parse(Request *request, StringView raw);

#endif
