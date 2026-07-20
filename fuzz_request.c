#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "jhttp.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	struct http_request req;
	char* buf = malloc(size);
	memcpy(buf, data, size);
	http_request_parse(&req, buf, size);
	free(buf);
	return 0;
}
