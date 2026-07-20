#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "jhttp.h"

static const char request_template[] =
    "GET /index.html?foo=bar HTTP/1.1\r\n"
    "Host: example.com\r\n"
    "User-Agent: benchmark/1.0\r\n"
    "Accept: */*\r\n"
    "Connection: keep-alive\r\n"
    "\r\n";

#ifndef ITERATIONS
#define ITERATIONS 1000000
#endif

int main(void)
{
    char buffer[sizeof(request_template)];
    struct http_request req;

    /* Warmup */
    for (int i = 0; i < 1000; ++i) {
        memcpy(buffer, request_template, sizeof(request_template));
        http_request_parse(&req, buffer);
    }

    clock_t start = clock();

    for (size_t i = 0; i < ITERATIONS; ++i) {
        memcpy(buffer, request_template, sizeof(request_template));

        /* Prevent the compiler from discarding the call. */
        volatile int result = http_request_parse(&req, buffer);
        (void)result;
    }

    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    printf("Iterations:      %zu\n", (size_t)ITERATIONS);
    printf("Elapsed:         %.6f s\n", elapsed);
    printf("Requests/sec:    %.0f\n", ITERATIONS / elapsed);
    printf("Time/request:    %.3f us\n",
           elapsed * 1e6 / ITERATIONS);

    return 0;
}
