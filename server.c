#include <signal.h>

#include "jhttp.h"

static int sig_exit = 0;
static void signal_handler(int sig) {
	printf("signal %d received\n", sig);
	sig_exit = 1;
}

int main(int argc, char** argv) {
	struct jhttp jhttp;

	if (jhttp_init(&jhttp) != 0) {
		fprintf(stderr, "failed to initialize jhttp\n");
		exit(1);
	}

	if (jhttp_listen(&jhttp, 6000) != 0) {
		fprintf(stderr, "failed to listen to port 6000\n");
		exit(1);
	}

	struct jhttp_connection* conn;
	for (;;) {
		jhttp_poll(&jhttp, &conn);
		if (!conn) continue;
		if (conn->body) {
			conn->len = snprintf(conn->buf, sizeof(conn->buf), "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
			conn->body = 0;
			conn->resp = 1;
		}
	}
}
