#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "jhttp.h"

static uint64_t ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

static volatile int sink;

int main(void)
{
    static const char request[] =
        "GET /index.html?x=1 HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "User-Agent: benchmark\r\n"
        "Accept: */*\r\n"
        "Connection: keep-alive\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

    enum {
        ITERATIONS = 10000000,
        POOL = 1024
    };

    char buffers[POOL][sizeof(request)];

    /* Prepare mutable copies */
    for (int i = 0; i < POOL; i++)
        memcpy(buffers[i], request, sizeof(request));

    struct http_request req;

    /* Warm-up (not timed) */
    for (int i = 0; i < 100000; i++) {
        char *buf = buffers[i & (POOL - 1)];
        memcpy(buf, request, sizeof(request));

	int rc;
        if ((rc = http_request_parse(&req, buf, sizeof(request) - 1)) != 0)
            abort();

        sink += rc;
    }

    uint64_t start = ns();

    /* Timed parse */
    for (int i = 0; i < ITERATIONS; i++) {
        char *buf = buffers[i & (POOL - 1)];

        /* Fresh mutable request */
        memcpy(buf, request, sizeof(request));

	int rc;
        if ((rc = http_request_parse(&req, buf, sizeof(request) - 1)) != 0)
            abort();

        sink += rc;
    }

    uint64_t end = ns();

    double seconds = (end - start) / 1e9;
    double ns_per = (double)(end - start) / ITERATIONS;
    double rps = ITERATIONS / seconds;
    double mibps =
        ((double)ITERATIONS * (sizeof(request) - 1)) /
        (1024.0 * 1024.0) / seconds;

    printf("iterations      : %d\n", ITERATIONS);
    printf("request size    : %zu bytes\n", sizeof(request) - 1);
    printf("time            : %.3f s\n", seconds);
    printf("requests/sec    : %.0f\n", rps);
    printf("ns/request      : %.2f\n", ns_per);
    printf("throughput      : %.2f MiB/s\n", mibps);
    printf("sink            : %d\n", sink);

    return 0;
}
