#ifndef JHTTP
#define JHTTP

#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>

#define JHTTP_HEADER_MAX     16
#define JHTTP_CONNECTION_MAX 256

struct jhttp_header {
	const char* key;
	const char* val;
};

struct jhttp_request {
	const char* method;
	const char* path;
	const char* version;
	const char* body;
	size_t header_count;
	struct jhttp_header headers[JHTTP_HEADER_MAX];
};

struct jhttp_response {
	size_t status;
	const char* buf;
};

struct jhttp_connection {
	int  socket;
	int  len;
	char buffer[8192];
};

struct jhttp {
	int                     (*callback)(const struct jhttp_request* req);
	int                     socket;
	struct sockaddr_in      addr;
	struct jhttp_connection conns[JHTTP_CONNECTION_MAX];
};

static int jhttp_request_parse(struct jhttp_request* req, char* str) {
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
		if (req->header_count >= JHTTP_HEADER_MAX) return -1;
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

static int jhttp_init(struct jhttp* jhttp, int port, int (*callback)(const struct jhttp_request* req)) {
	// handle null ptr
	if (!jhttp) return -1;

	// reset contents
	memset(jhttp, 0, sizeof(struct jhttp));

	jhttp->callback = callback;

	// setup socket
	jhttp->socket = socket(AF_INET, SOCK_STREAM, 0);
	if (jhttp->socket == -1) return -1;

	memset(&jhttp->addr, 0, sizeof(jhttp->addr));
	jhttp->addr.sin_family      = AF_INET;
	jhttp->addr.sin_port        = htons(port);
	jhttp->addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(jhttp->socket, (struct sockaddr*) &jhttp->addr, sizeof(jhttp->addr)) == -1) return -1;
	if (listen(jhttp->socket, SOMAXCONN) == -1) return -1;

	return 0;
}

static int jhttp_fini(struct jhttp* jhttp) {
	if (jhttp->socket) close(jhttp->socket);
	return 0;
}

static int jhttp_poll(struct jhttp* jhttp) {
	{
		size_t addr_len = sizeof(jhttp->addr);
		int s = accept(jhttp->socket, (struct sockaddr*) &jhttp->addr, (socklen_t*) &addr_len);
		if (s == -1) return -1;
		for (size_t i = 0; i < JHTTP_CONNECTION_MAX; i++)
			if (!jhttp->conns[i].socket) {
				jhttp->conns[i].socket = s;
				jhttp->conns[i].len = 0;
			}
	}
	struct jhttp_request req;
	for (size_t i = 0; i < JHTTP_CONNECTION_MAX; i++) {
		int   s = jhttp->conns[i].socket;
		int   l = jhttp->conns[i].len;
		char* b = jhttp->conns[i].buffer;
		if (!s)
			continue;
		int   r = read(s, b, sizeof(jhttp->conns[i].buffer) - l);
		if (r <= 0) {
			l = 0;
			close(s);
			continue;
		}
		jhttp->conns[i].len = r;
		printf("%d, %d\n", s, r);
		int   p = jhttp_request_parse(&req, b);
		if (p >= 0)
			jhttp->callback(&req);
		close(s);
		jhttp->conns[i].socket = 0;
		jhttp->conns[i].len = 0;
	}
	return 0;
}

#endif
