#ifndef JHTTP
#define JHTTP

#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

struct jhttp_header {
	char* key;
	char* val;
};

struct jhttp_request {
	char* method;
	char* path;
	char* query;
	char* version;
	char* body;
	struct jhttp_header headers[32];
};

struct jhttp_response {
	int  status;
	char body[8192];
};

struct jhttp_connection {
	int  socket;
	int  len;
	char buffer[8192];
};

struct jhttp {
	int                      (*callback)(struct jhttp_response* res, const struct jhttp_request* req);
	int                      socket;
	struct sockaddr_in       addr;
	size_t                   conn_capacity;
	struct jhttp_connection* conns;
};


static int jhttp_request_parse(struct jhttp_request* req, char* str) {
	struct jhttp_header* header = &req->headers[0];

	// skip leading empty line
	if (str[0] == '\r' && str[1] == '\n') str = str + 2;

	// method
	req->method = str;
	str = strchr(str, ' ');
	if (!str) return 400;
	*str = '\0';
	str++;

	// path
	req->path = str;
	str = strchr(str, ' ');
	if (!str) return 400;
	*str = '\0';
	str++;

	// query
	req->query = strchr(req->path, '?');
	if (req->query) {
		*req->query = '\0';
		req->query++;
	}

	// version
	req->version = str;
	str = strchr(str, '\r');
	if (!str) return 400;
	if (str[1] != '\n') return 400;
	*str = '\0';
	str++, str++;

	// headers
	while (1) {
		if (*str == '\r') break;
		// key
		header->key = NULL;
		if (header - req->headers >= sizeof(req->headers) / sizeof(req->headers[0]) - 1) return 431;
		header->key = str;
		str = strchr(str, ':');
		if (!str) return 400;
		*str = '\0';
		str++;

		// val
		while (isspace(*str)) str++;
		header->val = str;
		str = strchr(str, '\r');
		if (!str) return 400;
		if (str[1] != '\n') return 400;
		while (isspace(*(str - 1))) str--;
		*str = '\0';
		str++, str++;
		header++;
	}

	// request end validation
	if (str[0] != '\r' || str[1] != '\n') return 400;

	req->body = str + 2;
	return 0;
}

static int jhttp_init(struct jhttp* jhttp, int port, int (*callback)(struct jhttp_response* res, const struct jhttp_request* req)) {
	// handle null ptr
	if (!jhttp) return -1;

	// reset contents
	memset(jhttp, 0, sizeof(struct jhttp));
	jhttp->conn_capacity = 8192;
	jhttp->conns = malloc(jhttp->conn_capacity * sizeof(struct jhttp_connection));

	jhttp->callback = callback;

	// setup socket
	jhttp->socket = socket(AF_INET, SOCK_STREAM, 0);
	if (jhttp->socket == -1) return -1;

	// set non-blocking
	fcntl(jhttp->socket, F_SETFL, fcntl(jhttp->socket, F_GETFL, 0) | O_NONBLOCK);

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
	for (size_t i = 0; i < jhttp->conn_capacity; i++)
		if (jhttp->conns[i].socket)
			close(jhttp->conns[i].socket);
	if (jhttp->socket) close(jhttp->socket);
	if (jhttp->conns) free(jhttp->conns);
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
		for (size_t i = 0; i < jhttp->conn_capacity; i++)
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
	for (size_t i = 0; i < jhttp->conn_capacity; i++) {
		struct jhttp_connection* conn = &jhttp->conns[i];
		if (!conn->socket) continue;
		int r = read(conn->socket, conn->buffer + conn->len, sizeof(conn->buffer) - conn->len);
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
		printf("jhttp: connection %zu received %d bytes\r\n", i, r);
		conn->len += r;
		memset(&req, 0, sizeof(struct jhttp_request));
		char buf[sizeof(conn->buffer) + 1];
		memcpy(buf, conn->buffer, conn->len);
		buf[conn->len] = '\0';
		int s = jhttp_request_parse(&req, buf);
		if (s != 0) {
			close(conn->socket);
			memset(conn, 0, sizeof(struct jhttp_connection));
			continue;
		}
		size_t request_size = req.body - req.method;
		size_t content_length = 0;
		for (size_t i = 0; req.headers[i].key; i++)
			if (strcmp(req.headers[i].key, "Content-Length") == 0)
				content_length = atoi(req.headers[i].val);
		if (conn->len < request_size + content_length)
			continue;
		printf("jhttp: req size %zu content length %zu conn len %zu\n", request_size, content_length, conn->len);
		jhttp->callback(&res, &req);

		dprintf(conn->socket, "HTTP/1.1 %d\r\nContent-Length: %zu\r\n\r\n%s", res.status, strlen(res.body), res.body);
		conn->len = 0;
	}
	return 0;
}

#endif
