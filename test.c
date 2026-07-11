#include <assert.h>
#include <stdlib.h>

#include "jhttp.h"

void http_request_parse_incomplete() {
	const char* cases[] = {
		"",
		"G",
		"GET",
		"GET ",
		"GET /",
		"GET /ABC/",
		"GET / ",
		"GET / HTTP",
		"GET / HTTP/",
		"GET / HTTP/1.1",
		"GET / HTTP/1.1\r\n",
		"GET / HTTP/1.1\r\n\r"
	};

	for (int i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		http_request req;
		int rc = http_request_parse(&req, cases[i], strlen(cases[i]));
		if (rc != 0) {
			printf("[%d] \"%s\" not parsed as incomplete\n", i, cases[i]);
			exit(1);
		}
	}
}


int main(int argc, char** argv) {
	static void (*tests[])() = {
		http_request_parse_incomplete
	};

	for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
		tests[i]();
}
