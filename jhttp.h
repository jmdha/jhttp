#ifndef JHTTP
#define JHTTP

#include <asm-generic/errno.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

#define JHTTP_HEADER_MAX     16
#define JHTTP_CONNECTION_MAX 5000
#define JHTTP_RESPONSE_MAX   8192

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
	char   body[JHTTP_RESPONSE_MAX];
};

struct jhttp_connection {
	int  socket;
	int  len;
	char buffer[1024];
};

struct jhttp {
	int                     (*callback)(struct jhttp_response* res, const struct jhttp_request* req);
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

static int jhttp_init(struct jhttp* jhttp, int port, int (*callback)(struct jhttp_response* res, const struct jhttp_request* req)) {
	// handle null ptr
	if (!jhttp) return -1;

	// reset contents
	memset(jhttp, 0, sizeof(struct jhttp));

	jhttp->callback = callback;

	// setup socket
	jhttp->socket = socket(AF_INET, SOCK_STREAM, 0);
	if (jhttp->socket == -1) return -1;

	// set non-blocking
	int flags = fcntl(jhttp->socket, F_GETFL, 0);
	fcntl(jhttp->socket, F_SETFL, flags | O_NONBLOCK);

	// set reuse addr
	int opt = 1;
	setsockopt(jhttp->socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	// set address
	memset(&jhttp->addr, 0, sizeof(jhttp->addr));
	jhttp->addr.sin_family      = AF_INET;
	jhttp->addr.sin_port        = htons(port);
	jhttp->addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(jhttp->socket, (struct sockaddr*) &jhttp->addr, sizeof(jhttp->addr)) == -1) return -1;
	if (listen(jhttp->socket, SOMAXCONN) == -1) return -1;

	return 0;
}

static int jhttp_fini(struct jhttp* jhttp) {
	for (size_t i = 0; i < JHTTP_CONNECTION_MAX; i++)
		if (jhttp->conns[i].socket)
			close(jhttp->conns[i].socket);
	if (jhttp->socket) close(jhttp->socket);
	return 0;
}

static int jhttp_accept(struct jhttp* jhttp) {
	while (1) {
		socklen_t addr_len = sizeof(jhttp->addr);
		int s = accept(jhttp->socket, (struct sockaddr*) &jhttp->addr, &addr_len);
		if (s == -1 && errno != EAGAIN && errno != EWOULDBLOCK) return -1;
		if (s == -1 && (errno == EAGAIN || EWOULDBLOCK)) return 0;
		int flags = fcntl(s, F_GETFL, 0);
		if (flags == -1) {
			perror("fcntl");
			return -1;
		}
		if (fcntl(s, F_SETFL, flags | O_NONBLOCK) == -1) {
			perror("fcntl");
			return -1;
		}
		for (size_t i = 0; i < JHTTP_CONNECTION_MAX; i++)
			if (!jhttp->conns[i].socket) {
				jhttp->conns[i].socket = s;
				jhttp->conns[i].len = 0;
				break;
			}
	}
}

static int jhttp_poll(struct jhttp* jhttp) {
	if (jhttp_accept(jhttp) != 0) return -1;
	struct jhttp_request  req;
	struct jhttp_response res;
	for (size_t i = 0; i < JHTTP_CONNECTION_MAX; i++) {
		struct jhttp_connection* conn = &jhttp->conns[i];
		if (!conn->socket) continue;
		int r = read(conn->socket, conn->buffer, sizeof(conn->buffer) - conn->len);
		if (r < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
			close(conn->socket);
			memset(conn, 0, sizeof(struct jhttp_connection));
			continue;
		}
		if (r == 0) {
			close(conn->socket);
			memset(conn, 0, sizeof(struct jhttp_connection));
			continue;
		}
		conn->len += r;
		memset(&req, 0, sizeof(struct jhttp_request));
		char buf[sizeof(conn->buffer) + 1];
		memcpy(buf, conn->buffer, conn->len);
		buf[conn->len] = '\0';
		int s = jhttp_request_parse(&req, buf);
		if (s < 0) {
			close(conn->socket);
			memset(conn, 0, sizeof(struct jhttp_connection));
			continue;
		}
		if (!req.body)
			continue;
		jhttp->callback(&res, &req);

		size_t len = 0;
		char obuf[JHTTP_RESPONSE_MAX];
		switch (res.status) {
		case 200: len = snprintf(obuf, sizeof(obuf), "HTTP/1.1 200 OK\r\n"); break;
		case 400: len = snprintf(obuf, sizeof(obuf), "HTTP/1.1 400 Bad Request\r\n"); break;
		case 404: len = snprintf(obuf, sizeof(obuf), "HTTP/1.1 404 Not Found\r\n"); break;
		}
		len += snprintf(obuf + len, sizeof(obuf) - len, "Content-Length: %zu\r\n\r\n", strlen(res.body));
		len += snprintf(obuf + len, sizeof(obuf) - len, "%s", res.body);
		size_t sent = 0;
		while (sent < len) {
			size_t n = write(conn->socket, obuf, len);
			if (n <= 0)
				break;
			sent += n;
		}
		conn->len = 0;
	}
	return 0;
}

#endif
