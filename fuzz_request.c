#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "jhttp.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	http_request req;
	http_request_parse(&req, (const char*) data, size);
	return 0;
}
