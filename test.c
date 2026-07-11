#include <assert.h>
#include <stdlib.h>

#include "jhttp.h"

void http_request_parse_incomplete() {
	printf("----http_request_parse_incomplete----\n");
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
		"GET / HTTP/1.1\r\n\r",
		"GET / HTTP/1.1\r\nABC",
		"GET / HTTP/1.1\r\nABC:",
		"GET / HTTP/1.1\r\nABC: ",
		"GET / HTTP/1.1\r\nABC: ABC",
		"GET / HTTP/1.1\r\nABC: ABC\r",
		"GET / HTTP/1.1\r\nABC: ABC\r\n",
		"GET / HTTP/1.1\r\nABC: ABC\r\n\r",
	};

	for (int i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		http_request req = { 0 };
		int rc = http_request_parse(&req, cases[i], strlen(cases[i]));
		if (rc != 0) {
			printf("[%d] %d \"%s\" not parsed as incomplete\n", i, rc, cases[i]);
			exit(1);
		}
	}
	printf("----DONE\n----");
}


int main(int argc, char** argv) {
	static void (*tests[])() = {
		http_request_parse_incomplete
	};

	for (int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
		tests[i]();
}
