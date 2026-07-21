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
	req->header_count = 0;

	// skip leading empty line
	if (str[0] == '\r' && str[1] == '\n') str = str + 2;

	// method
	req->method = str;
	str = (char*) strchr(str, ' ');
	if (!str) return -1;
	*str = '\0';
	str++;

	// path
	req->path = str;
	str = (char*) strchr(str, ' ');
	if (!str) return -1;
	*str = '\0';
	str++;

	// version
	req->version = str;
	str = (char*) strchr(str, '\r');
	if (!str) return -1;
	*str = '\0';
	if (str[1] != '\n') return -1;
	str++, str++;

	// headers
	while (1) {
		if (*str == '\r') break;
		// key
		if (req->header_count >= HTTP_HEADER_MAX) return -1;
		req->headers[req->header_count].key = str;
		str = strchr(str, ':');
		if (!str) return -1;
		*str = '\0';
		str++;

		// val
		while (isspace(*str)) str++;
		req->headers[req->header_count].val = str;
		str = (char*) strchr(str, '\r');
		if (!str) return -1;
		if (str[1] != '\n') return -1;
		str++, str++;
		req->header_count++;
	}

	// request end validation
	if (str[0] != '\r' || str[1] != '\n') return -1;

	req->body = str + 2;
	return 0;
}

static void http_listen(int port) {
}

#endif
