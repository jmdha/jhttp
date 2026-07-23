#include "jhttp.h"
#include <signal.h>

struct route {
	char* method;
	char* path;
	int (*fn)(struct jhttp_response* res, const struct jhttp_request* req);
};

int get_index(struct jhttp_response* res, const struct jhttp_request* req) {
	const char str[] =
		"<!DOCTYPE html>"
		"<html>"
		"	<body><p>Hello!</p></body>"
		"</html>k";
	snprintf(res->body, JHTTP_RESPONSE_MAX, "%s", str);
	res->status = 200;
	return 0;
}

const struct route routes[] = {
	{ "GET",  "/", get_index },
};

int handler(struct jhttp_response* res, const struct jhttp_request* req) {
	for (size_t i = 0; i < sizeof(routes) / sizeof(struct route); i++)
		if (strcmp(req->method, routes[i].method) == 0 && 
		    strcmp(req->path,   routes[i].path)   == 0)
			return routes[i].fn(res, req);
	return 0;
}

static int sig_exit = 0;
static void signal_handler(int sig) {
	printf("signal %d received\n", sig);
	sig_exit = 1;
}

int main(int argc, char** argv) {
	struct jhttp jhttp;

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGKILL, signal_handler);
	signal(SIGQUIT, signal_handler);
	signal(SIGABRT, signal_handler);

	if (jhttp_init(&jhttp, 8000, handler) != 0) {
		fprintf(stderr, "failed to initialize jhttp on port 8000\n");
		exit(1);
	}

	while (sig_exit == 0 && jhttp_poll(&jhttp) == 0) {
	}
	jhttp_fini(&jhttp);
}
