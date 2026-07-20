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

static int http_request_parse(struct http_request* req, char* str) {
	char* ptr;
	req->header_count = 0;

	// skip leading empty line
	if (str[0] == '\r' && str[1] == '\n') str = str + 2;

	// method
	ptr = (char*) strchr(str, ' ');
	if (!ptr) return -1;
	req->method = str;
	*ptr = '\0';
	str = ptr + 1;

	// path
	ptr = (char*) strchr(str, ' ');
	if (!ptr) return -1;
	req->path = str;
	*ptr = '\0';
	str = ptr + 1;

	// version
	ptr = (char*) strchr(str, '\r');
	if (!ptr) return -1;
	req->version = str;
	*ptr = '\0';
	if (ptr[1] != '\n') return -1;
	str = ptr + 2;

	// headers
	while (1) {
		if (*str == '\r') break;
		// key
		ptr = (char*) strchr(str, ':');
		if (!ptr) break;
		if (req->header_count >= HTTP_HEADER_MAX) return -1;
		req->headers[req->header_count].key = str;
		*ptr = '\0';
		str = ptr + 1;
		while (isspace(*str)) str++;

		// val
		ptr = (char*) strchr(str, '\r');
		if (!ptr) return -1;
		req->headers[req->header_count].val = str;
		str = ptr + 1;
		while (isspace(*(ptr - 1))) ptr--;
		*ptr = '\0';
		if (*str != '\n') return -1;
		str++;
		req->header_count++;
	}

	// request end validation
	if (str[0] != '\r' || str[1] != '\n') return -1;

	str++, str++;
	req->body = str;
	return 0;
}

static void http_listen(int port) {
}

#endif
