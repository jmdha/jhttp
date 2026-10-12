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


struct jhttp_connection {
	int socket;
	size_t ptr;
	size_t len;
	char buf[8192];
	struct sockaddr_in addr;
	unsigned char open : 1; // listening for connections
	unsigned char head : 1; // reading header
	unsigned char body : 1; // reading body
	unsigned char resp : 1; // writing response
};

struct jhttp {
	size_t conn_len;
	size_t conn_cap;
	struct jhttp_connection* conns;
};

// parse string as http header
// returns -1 in case of invalid request, 0 in case of incomplete, n in case of success where n is the number of bytes parsed
static int jhttp_request_parse(char* str, char** method, char** path, char** query, char** version) {
	const char* start = str;

	// skip leading empty line
	if (str[0] == '\r' && str[1] == '\n') str = str + 2;

	// method
	*method = str;
	str = strchr(str, ' ');
	if (!str) return 0;
	*str = '\0';
	str++;

	// path
	*path = str;
	str = strchr(str, ' ');
	if (!str) return 0;
	*str = '\0';
	str++;

	// query
	*query = strchr(*path, '?');
	if (*query) {
		**query = '\0';
		(*query)++;
	}

	// version
	*version = str;
	str = strchr(str, '\r');
	if (!str) return 0;
	if (str[1] != '\n') return -1;
	*str = '\0';
	str++, str++;

	// request end validation
	if (str[0] != '\r' || str[1] != '\n') return -1;

	return (str + 2) - start;
}

int jhttp_init(struct jhttp* jhttp) {
	memset(jhttp, 0, sizeof(struct jhttp));
	return 0;
}

int jhttp_fini(struct jhttp* jhttp) {

}

int jhttp_new_connection(struct jhttp* jhttp, struct jhttp_connection** conn) {
	if (jhttp->conn_len >= jhttp->conn_cap) {
		jhttp->conn_cap = jhttp->conn_cap == 0 ? 8 : 2 * jhttp->conn_cap;
		jhttp->conns = realloc(jhttp->conns, jhttp->conn_cap * sizeof(struct jhttp_connection));
		if (!jhttp->conns) {
			fprintf(stderr, "jhttp: out of memory\n");
			return -1;
		}
	}
	for (size_t i = 0; i < jhttp->conn_len; i++) {
		if (jhttp->conns[i].socket)
			continue;
		*conn = &jhttp->conns[i];
		memset(*conn, 0, sizeof(struct jhttp_connection));
		return 0;
	}
	*conn = &jhttp->conns[jhttp->conn_len];
	jhttp->conn_len++;
	memset(*conn, 0, sizeof(struct jhttp_connection));
	return 0;
}

int jhttp_listen(struct jhttp* jhttp, int port) {
	struct jhttp_connection* conn;
	if (jhttp_new_connection(jhttp, &conn) != 0) return -1;
	conn->open = 1;

	conn->socket = socket(AF_INET, SOCK_STREAM, 0);
	if (conn->socket == -1) return -1;

	fcntl(conn->socket, F_SETFL, fcntl(conn->socket, F_GETFL, 0) | O_NONBLOCK);

	int opt = 1;
	setsockopt(conn->socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family      = AF_INET;
	addr.sin_port        = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(conn->socket, (struct sockaddr*) &addr, sizeof(addr)) == -1) return -1;
	if (listen(conn->socket, SOMAXCONN) == -1) return -1;

	return 0;
}

// NOTE: conn remains valid until next call of jhttp_poll
int jhttp_poll(struct jhttp* jhttp, struct jhttp_connection** oconn) {
	*oconn = NULL;
	// TODO: use epoll instead of iteration
	for (size_t i = 0; i < jhttp->conn_len; i++) {
		struct jhttp_connection* conn = &jhttp->conns[i];
		if (conn->open) {
			struct sockaddr_in addr;
			socklen_t addr_len = sizeof(addr);
			int s = accept(conn->socket, (struct sockaddr*) &addr, &addr_len);
			if (s == -1) continue;

			fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
			if (jhttp_new_connection(jhttp, &conn) != 0) {
				fprintf(stderr, "jhttp: failed to create connection\n");
				continue;
			}
			printf("jhttp [%d]: new connection\n", s);
			memset(conn, 0, sizeof(*conn));
			conn->socket = s;
			conn->addr = addr;
			conn->head = 1;
			continue;
		}

		if (conn->resp) {
			int r = send(conn->socket, conn->buf + conn->ptr, conn->len - conn->ptr, MSG_NOSIGNAL);
			if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
				continue;
			if (r == 0 || r < 0) {
				close(conn->socket);
				memset(conn, 0, sizeof(struct jhttp_connection));
				continue;
			}
			conn->ptr += r;
			if (conn->ptr < conn->len)
				continue;
			conn->resp = 0;
			conn->head = 1;
			conn->ptr = 0;
			conn->len = 0;
			*oconn = conn;
			return 0;
		}

		if (conn->head) {
			int r = recv(conn->socket, conn->buf + conn->len, sizeof(conn->buf) - conn->len, MSG_DONTWAIT);
			if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR))
				continue;
			if (r == 0 || r < 0) {
				close(conn->socket);
				memset(conn, 0, sizeof(struct jhttp_connection));
				continue;
			}
			printf("jhttp [%d]: received %d bytes\n", conn->socket, r);
			conn->len += r;
			// TODO: try parse
			char* bpos = strstr(conn->buf, "\r\n\r\n");
			if (bpos == NULL)
				continue;
			conn->head = 0;
			conn->body = 1;

			*oconn = conn;
			return 0;
		}
	}
	return 0;
}

#endif
