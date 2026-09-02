#include "ceasy/ceasy.h"

#include <assert.h>

int main(void) {
    Request request;
    RequestParseResult result = request_parse(
        &request, sv("GET /posts?x=1 HTTP/1.1\r\nHost: localhost\r\n\r\n"));

    assert(result == REQUEST_PARSE_OK);
    assert(request.method == HTTP_METHOD_GET);
    assert(stringv_equal(request.path, sv("/posts")));
    assert(
        stringv_equal(request_header(&request, sv("host")), sv("localhost")));
    result = request_parse(
        &request,
        sv("POST /posts HTTP/1.1\r\ncontent-length: 11\r\nContent-Type: "
           "application/x-www-form-urlencoded\r\n\r\ntitle=Hello"));
    assert(result == REQUEST_PARSE_OK);
    assert(request.content_length == 11);
    assert(stringv_equal(request.body, sv("title=Hello")));
    assert(request_header(&request, sv("CONTENT-TYPE")).length > 0);
    assert(
        request_parse(&request,
                      sv("POST / HTTP/1.1\r\nContent-Length: nope\r\n\r\n")) ==
        REQUEST_PARSE_BAD_REQUEST);
    assert(request_parse(
               &request,
               sv("POST / HTTP/1.1\r\nContent-Length: 1048577\r\n\r\n")) ==
           REQUEST_PARSE_PAYLOAD_TOO_LARGE);
    assert(request_parse(
               &request,
               sv("POST / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n")) ==
           REQUEST_PARSE_UNSUPPORTED);
    return 0;
}
