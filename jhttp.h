#ifndef JHTTP
#define JHTTP

#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>

enum jhttp_status {
	JHTTP_STATUS_CONTINUE                = 100, // RFC 9110, 15.2.1
	JHTTP_STATUS_SWITCHINGPROTOCOLS      = 101, // RFC 9110, 15.2.2

	JHTTP_STATUS_OK                      = 200, // RFC 9110, 15.3.1
	JHTTP_STATUS_CREATED                 = 201, // RFC 9110, 15.3.2
	JHTTP_STATUS_ACCEPTED                = 202, // RFC 9110, 15.3.3
	JHTTP_STATUS_NONAUTHORITATIVEINFO    = 203, // RFC 9110, 15.3.4
	JHTTP_STATUS_NOCONTENT               = 204, // RFC 9110, 15.3.5
	JHTTP_STATUS_RESETCONTENT            = 205, // RFC 9110, 15.3.6
	JHTTP_STATUS_PARTIALCONTENT          = 206, // RFC 9110, 15.3.7

	JHTTP_STATUS_MULTIPLECHOICES         = 300, // RFC 9110, 15.4.1
	JHTTP_STATUS_MOVEDPERMANENTLY        = 301, // RFC 9110, 15.4.2
	JHTTP_STATUS_FOUND                   = 302, // RFC 9110, 15.4.3
	JHTTP_STATUS_SEEOTHER                = 303, // RFC 9110, 15.4.4
	JHTTP_STATUS_NOTMODIFIED             = 304, // RFC 9110, 15.4.5
	JHTTP_STATUS_USEPROXY                = 305, // RFC 9110, 15.4.6
	JHTTP_STATUS_TEMPORARYREDIRECT       = 307, // RFC 9110, 15.4.8
	JHTTP_STATUS_PERMANENTREDIRECT       = 308, // RFC 9110, 15.4.9

	JHTTP_STATUS_BADREQUEST              = 400, // RFC 9110, 15.5.1
	JHTTP_STATUS_UNAUTHORIZED            = 401, // RFC 9110, 15.5.2
	JHTTP_STATUS_PAYMENTREQUIRED         = 402, // RFC 9110, 15.5.3
	JHTTP_STATUS_FORBIDDEN               = 403, // RFC 9110, 15.5.4
	JHTTP_STATUS_NOTFOUND                = 404, // RFC 9110, 15.5.5
	JHTTP_STATUS_METHODNOTALLOWED        = 405, // RFC 9110, 15.5.6
	JHTTP_STATUS_NOTACCEPTABLE           = 406, // RFC 9110, 15.5.7
	JHTTP_STATUS_REQUESTTIMEOUT          = 408, // RFC 9110, 15.5.9
	JHTTP_STATUS_CONFLICT                = 409, // RFC 9110, 15.5.10
	JHTTP_STATUS_GONE                    = 410, // RFC 9110, 15.5.11
	JHTTP_STATUS_LENGTHREQUIRED          = 411, // RFC 9110, 15.5.12
	JHTTP_STATUS_PRECONDITIONFAILED      = 412, // RFC 9110, 15.5.13
	JHTTP_STATUS_CONTENTTOOLARGE         = 413, // RFC 9110, 15.5.14
	JHTTP_STATUS_URITOOLONG              = 414, // RFC 9110, 15.5.15
	JHTTP_STATUS_UNSUPPORTEDMEDIATYPE    = 415, // RFC 9110, 15.5.16
	JHTTP_STATUS_RANGENOTSATISFIABLE     = 416, // RFC 9110, 15.5.17
	JHTTP_STATUS_EXPECTATIONFAILED       = 417, // RFC 9110, 15.5.18
	JHTTP_STATUS_MISDIRECTEDREQUEST      = 421, // RFC 9110, 15.5.20
	JHTTP_STATUS_UNPROCESSABLECONTENT    = 422, // RFC 9110, 15.5.21
	JHTTP_STATUS_UPGRADEREQUIRED         = 426, // RFC 9110, 15.5.22

	JHTTP_STATUS_INTERNALSERVERERROR     = 500, // RFC 9110, 15.6.1
	JHTTP_STATUS_NOTIMPLEMENTED          = 501, // RFC 9110, 15.6.2
	JHTTP_STATUS_BADGATEWAY              = 502, // RFC 9110, 15.6.3
	JHTTP_STATUS_SERVICEUNAVAILABLE      = 503, // RFC 9110, 15.6.4
	JHTTP_STATUS_GATEWAYTIMEOUT          = 504, // RFC 9110, 15.6.5
	JHTTP_STATUS_HTTPVERSIONNOTSUPPORTED = 505, // RFC 9110, 15.6.6
	
};

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
	struct jhttp_header headers[8];
};

struct jhttp_response {
	size_t status;
	char   body[8192];
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
	char* ptr;
	struct jhttp_header* header = &req->headers[0];
	header->key = NULL;

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
		if (header - req->headers >= sizeof(req->headers) / sizeof(req->headers[0])) return 431;
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
		char obuf[sizeof(res.body)];
		switch (res.status) {
		case 200: len = snprintf(obuf, sizeof(obuf), "HTTP/1.1 200 OK\r\n"); break;
		case 400: len = snprintf(obuf, sizeof(obuf), "HTTP/1.1 400 Bad Request\r\n"); break;
		case 404: len = snprintf(obuf, sizeof(obuf), "HTTP/1.1 404 Not Found\r\n"); break;
		case 409: len = snprintf(obuf, sizeof(obuf), "HTTP/1.1 409 Conflict\r\n"); break;
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
