#ifndef JHTTP
#define JHTTP

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define HTTP_METHOD_MAX     16
#define HTTP_PATH_MAX       512
#define HTTP_VERSION_MAX    16
#define HTTP_HEADER_KEY_MAX 64
#define HTTP_HEADER_VAL_MAX 256
#define HTTP_HEADER_MAX     64

typedef struct http_header {
	char        key[HTTP_HEADER_KEY_MAX];
	char        val[HTTP_HEADER_VAL_MAX];
} http_header;

typedef struct http_request {
	char        method[HTTP_METHOD_MAX];
	char        path[HTTP_PATH_MAX];
	char        version[HTTP_VERSION_MAX];
	int         header_count;
	http_header headers[HTTP_HEADER_MAX];
} http_request;

typedef struct http_response {

} http_response;

// Parses a http request
// return of a negative number means error
// return of a 0 means needs more input
// return of a positive number means success,
// 	and the number is the number of bytes parsed
static int http_request_parse(http_request* req, const char* buf, int size) {
	int r = 0;
	while (1) {
		if (r >= size)
			return 0;
		if (buf[r] == '\n')
			break;
		r++;
	}
	if (r <= 0 || buf[r - 1] != '\r')
		return -1;
	char tmp[8192];
	sprintf(tmp, "%.*s", r, buf);
	if (sscanf(tmp, "%15s %511s %15s", req->method, req->path, req->version) != 3)
		return -1;

	int s = r;
	while (1) {
		r++;
		if (req->header_count >= HTTP_HEADER_MAX)
			return -1;
		if (r >= size)
			return 0;
		if (buf[r] == '\n') {
			if (s == r || buf[r - 1] != '\r')
				return -1;
			if (buf[r - 2] == '\n' && buf[r - 3] == '\r')
				return r;
			http_header* header = &req->headers[req->header_count];
			sprintf(tmp, "%.*s", r - s, &buf[s]);
			if (sscanf(tmp, "%63[^:]: %255s", header->key, header->val) != 2)
				return -1;
			req->header_count++;
			for (size_t i = req->header_count - 1; i > 0 && i <= HTTP_HEADER_MAX; i--)
				if (strcmp(req->headers[i].key, req->headers[i - 1].key) == 0)
					return -1;
			s = r;
			continue;
		}
	}

	return r;
}

static void http_listen(int port) {
}

#endif
