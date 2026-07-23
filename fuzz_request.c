#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "jhttp.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
	struct jhttp_request req;
	char* buf = malloc(size + 1);
	memcpy(buf, data, size);
	buf[size] = '\0';
	jhttp_request_parse(&req, buf);
	free(buf);
	return 0;
}
