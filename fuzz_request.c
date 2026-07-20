#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "jhttp.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	struct http_request req;
	char* buf = malloc(size + 1);
	memcpy(buf, data, size);
	buf[size] = '\0';
	http_request_parse(&req, buf);
	free(buf);
	return 0;
}
