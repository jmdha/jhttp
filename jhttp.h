#ifndef JHTTP
#define JHTTP

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define HTTP_HEADER_MAX 16
#define HTTP_RESP_MAX   1 * 1024 * 1024

struct http_header {
	const char* key;
	const char* val;
};

struct http_request {
	const char* method;
	const char* path;
	const char* version;
	const char* body;
	size_t header_count;
	struct http_header headers[HTTP_HEADER_MAX];
};

struct http_response {
	size_t status;
	char   buf[HTTP_RESP_MAX];
};

static int http_request_parse(struct http_request* req, char* buf, int len) {
	char* ptr;
	req->header_count = 0;

	// method
	if (len == 0) return -1;
	ptr = memchr(buf, ' ', len);
	if (!ptr) return -1;
	*ptr = '\0';
	req->method = buf;
	len -= (ptr - buf) + 1;
	buf = ptr + 1;

	// path
	if (len == 0) return -1;
	ptr = memchr(buf, ' ', len);
	if (!ptr) return -1;
	*ptr = '\0';
	req->path = buf;
	len -= (ptr - buf) + 1;
	buf = ptr + 1;

	// version
	if (len == 0) return -1;
	ptr = memchr(buf, '\r', len);
	if (!ptr) return -1;
	*ptr = '\0';
	req->version = buf;
	len -= (ptr - buf) + 1;
	buf = ptr + 1;
	if (len == 0 || *buf != '\n') return -1;
	buf++;
	len--;

	// headers
	while (1) {
		// key
		ptr = memchr(buf, ':', len);
		if (!ptr) break;
		if (req->header_count >= HTTP_HEADER_MAX) return -1;
		*ptr = '\0';
		req->headers[req->header_count].key = buf;
		len -= (ptr - buf) + 1;
		buf = ptr + 1;
		while (len > 0 && isspace(*buf)) buf++, len--;

		// val
		if (len == 0) return -1;
		ptr = memchr(buf, '\r', len);
		if (!ptr) return -1;
		*ptr = '\0';
		req->headers[req->header_count].val = buf;
		len -= (ptr - buf) + 1;
		buf = ptr + 1;
		if (len == 0 || *buf != '\n') return -1;
		buf++;
		len--;
		req->header_count++;
	}

	// request end validation
	if (len < 2) return -1;
	if (buf[0] != '\r' || buf[1] != '\n') return -1;

	buf++, buf++;
	req->body = buf;
	return 0;
}

static void http_listen(int port) {
}

#endif
