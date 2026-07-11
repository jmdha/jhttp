#include <criterion/criterion.h>

#include "jhttp.h"

Test(http_request_parse, empty) {
	const char* buf = "";
	http_request req;
	int rc;

	rc = http_request_parse(&req, buf, strlen(buf));

	cr_expect(rc == 0);
}

Test(http_request_parse, statusline) {
	const char* buf = "GET / HTTP/1.1\r\n";
	http_request req;
	int rc;

	rc = http_request_parse(&req, buf, strlen(buf));

	cr_assert(rc >= 0);
	cr_expect(strcmp(req.method,  "GET")      == 0);
	cr_expect(strcmp(req.path,    "/")        == 0);
	cr_expect(strcmp(req.version, "HTTP/1.1") == 0);
}

Test(http_request_parse, minimal) {
	const char* buf = "GET / HTTP/1.1\r\nHost: google.com\r\n\r\n";
	http_request req;
	int rc;

	rc = http_request_parse(&req, buf, strlen(buf));

	cr_assert(rc >= 0);
	cr_expect(strcmp(req.method,  "GET")      == 0);
	cr_expect(strcmp(req.path,    "/")        == 0);
	cr_expect(strcmp(req.version, "HTTP/1.1") == 0);

	cr_assert(req.header_count == 1);
	cr_expect(strcmp(req.headers[0].key, "Host")       == 0);
	cr_expect(strcmp(req.headers[0].val, "google.com") == 0);
}

Test(http_request_parse, multipleheader) {
	const char* buf = "GET / HTTP/1.1\r\nHost: google.com\r\nABC: qwe\r\n\r\n";
	http_request req;
	int rc;

	rc = http_request_parse(&req, buf, strlen(buf));

	cr_assert(rc >= 0);
	cr_expect(strcmp(req.method,  "GET")      == 0);
	cr_expect(strcmp(req.path,    "/")        == 0);
	cr_expect(strcmp(req.version, "HTTP/1.1") == 0);

	cr_assert(req.header_count == 2);
	cr_expect(strcmp(req.headers[0].key, "Host")       == 0);
	cr_expect(strcmp(req.headers[0].val, "google.com") == 0);
	cr_expect(strcmp(req.headers[1].key, "ABC")        == 0);
	cr_expect(strcmp(req.headers[1].val, "qwe")        == 0);
}

Test(http_request_parse, duplicateheader) {
	const char* buf = "GET / HTTP/1.1\r\nHost: google.com\r\nHost: google.com\r\n\r\n";
	http_request req;
	int rc;

	rc = http_request_parse(&req, buf, strlen(buf));

	cr_assert(rc < 0);
}
