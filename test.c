#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jhttp.h"

static int test_pass, test_fail, test_failed;
static const char* test_name;

#define TEST(name) static void name(void)

#define EXPECTm(cond, msg) do { \
	if (cond) break; \
	printf("FAIL %s: %s (%s:%d)\n", test_name, msg, __FILE__, __LINE__); \
	test_failed = 1; \
	return; \
} while (0)

#define EXPECT(cond) EXPECTm(cond, #cond)

#define EXPECT_EQm(exp, act, msg) do { \
	long e = (long)(exp), a = (long)(act); \
	if (e == a) break; \
	printf("FAIL %s: %s — expected %ld got %ld (%s:%d)\n", \
	       test_name, msg, e, a, __FILE__, __LINE__); \
	test_failed = 1; \
	return; \
} while (0)

#define EXPECT_STRm(exp, act, msg) do { \
	const char *e = (exp), *a = (act); \
	if (strcmp(e, a) == 0) break; \
	printf("FAIL %s: %s — expected [%s] got [%s] (%s:%d)\n", \
	       test_name, msg, e, a, __FILE__, __LINE__); \
	test_failed = 1; \
	return; \
} while (0)

#define RUN(fn) do { \
	test_name = #fn; \
	test_failed = 0; \
	fn(); \
	if (test_failed) test_fail++; else test_pass++; \
} while (0)

/* Exact-size heap copy so ASan catches any read/write past len */
static char* cur;

static int parse(const char* raw, struct http_request* req)
{
	free(cur);
	cur = malloc(strlen(raw) + 1);
	strcpy(cur, raw);
	return http_request_parse(req, cur);
}

/* ---- functional ---- */

TEST(request_line)
{
	struct http_request req;
	EXPECT_EQm(0, parse("GET / HTTP/1.1\r\n\r\n", &req), "parse");
	EXPECT_STRm("GET", req.method, "method");
	EXPECT_STRm("/", req.path, "path");
	EXPECT_STRm("HTTP/1.1", req.version, "version");
	EXPECT_EQm(0, req.header_count, "header count");
}

TEST(path_with_query)
{
	struct http_request req;
	EXPECT_EQm(0, parse("GET /index.html?x=1&y=2 HTTP/1.1\r\n\r\n", &req), "parse");
	EXPECT_STRm("/index.html?x=1&y=2", req.path, "path");
}

TEST(single_header)
{
	struct http_request req;
	EXPECT_EQm(0, parse("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n", &req), "parse");
	EXPECT_EQm(1, req.header_count, "header count");
	EXPECT_STRm("Host", req.headers[0].key, "key");
	EXPECT_STRm("example.com", req.headers[0].val, "val");
}

TEST(multiple_headers)
{
	struct http_request req;
	EXPECT_EQm(0, parse("GET / HTTP/1.1\r\n"
	                    "Host: example.com\r\n"
	                    "Accept: */*\r\n"
	                    "Connection: keep-alive\r\n"
	                    "\r\n", &req), "parse");
	EXPECT_EQm(3, req.header_count, "header count");
	EXPECT_STRm("Host", req.headers[0].key, "key 0");
	EXPECT_STRm("example.com", req.headers[0].val, "val 0");
	EXPECT_STRm("Accept", req.headers[1].key, "key 1");
	EXPECT_STRm("*/*", req.headers[1].val, "val 1");
	EXPECT_STRm("Connection", req.headers[2].key, "key 2");
	EXPECT_STRm("keep-alive", req.headers[2].val, "val 2");
}

static void build_request(char* buf, int nheaders)
{
	int off = sprintf(buf, "GET / HTTP/1.1\r\n");
	for (int i = 0; i < nheaders; i++)
		off += sprintf(buf + off, "H%d: v\r\n", i);
	sprintf(buf + off, "\r\n");
}

TEST(header_count_max)
{
	char raw[1024];
	struct http_request req;
	build_request(raw, HTTP_HEADER_MAX);
	EXPECT_EQm(0, parse(raw, &req), "parse");
	EXPECT_EQm(HTTP_HEADER_MAX, req.header_count, "header count");
}

TEST(header_count_overflow)
{
	char raw[1024];
	struct http_request req;
	build_request(raw, HTTP_HEADER_MAX + 1);
	EXPECT_EQm(-1, parse(raw, &req), "one header past max");
}

TEST(bare_lf_line_endings)
{
	struct http_request req;
	EXPECT_EQm(-1, parse("GET / HTTP/1.1\nHost: x\n\n", &req), "bare LF");
}

TEST(empty_input)
{
	struct http_request req;
	EXPECT_EQm(-1, parse("", &req), "empty");
}

/* Known bug: body points at the terminating CRLF instead of past it */
TEST(body_starts_after_blank_line)
{
	struct http_request req;
	EXPECT_EQm(0, parse("POST / HTTP/1.1\r\nHost: x\r\n\r\nhello", &req), "parse");
	EXPECTm(memcmp(req.body, "hello", 5) == 0,
	        "body must not include the blank-line CRLF");
}

TEST(incomplete_is_rejected)
{
	static const char* cases[] = {
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

	struct http_request req;
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
		EXPECT_EQm(-1, parse(cases[i], &req), cases[i]);
}

/* ---- conformance: valid messages a parser must (or may) accept ---- */

/* RFC 9112 §5: field-line = field-name ":" OWS field-value OWS
 * OWS is *zero* or more SP/HTAB */
TEST(accept_no_ows_after_colon)
{
	struct http_request req;
	EXPECT_EQm(0, parse("GET / HTTP/1.1\r\nHost:example.com\r\n\r\n", &req),
	           "RFC 9112 §5: OWS after colon is optional");
	EXPECT_STRm("example.com", req.headers[0].val, "val");
}

/* RFC 9112 §5: OWS = SP / HTAB */
TEST(accept_htab_after_colon)
{
	struct http_request req;
	EXPECT_EQm(0, parse("GET / HTTP/1.1\r\nHost:\texample.com\r\n\r\n", &req),
	           "RFC 9112 §5: HTAB is valid OWS after colon");
	EXPECT_STRm("example.com", req.headers[0].val, "val");
}

/* RFC 9110 §5.5: "a field parsing implementation MUST exclude such
 * whitespace prior to evaluating the field value" */
TEST(accept_and_strip_surrounding_ows)
{
	struct http_request req;
	EXPECT_EQm(0, parse("GET / HTTP/1.1\r\nHost:   example.com  \r\n\r\n", &req),
	           "parse");
	EXPECT_STRm("example.com", req.headers[0].val,
	            "RFC 9110 §5.5: OWS MUST be excluded from value");
}

/* RFC 9110 §5.5: field-value = *field-content — empty value is valid */
TEST(accept_empty_field_value)
{
	struct http_request req;
	EXPECT_EQm(0, parse("GET / HTTP/1.1\r\nX:\r\nHost: h\r\n\r\n", &req),
	           "RFC 9110 §5.5: empty field value is valid");
}

/* RFC 9112 §3.2.4: asterisk-form = "*", for server-wide OPTIONS */
TEST(accept_asterisk_form)
{
	struct http_request req;
	EXPECT_EQm(0, parse("OPTIONS * HTTP/1.1\r\nHost: h\r\n\r\n", &req),
	           "RFC 9112 §3.2.4: asterisk-form request-target");
	EXPECT_STRm("*", req.path, "path");
}

/* RFC 9112 §3.2.2: "A server MUST accept the absolute-form in requests" */
TEST(accept_absolute_form)
{
	struct http_request req;
	EXPECT_EQm(0, parse("GET http://example.com/x HTTP/1.1\r\nHost: h\r\n\r\n", &req),
	           "RFC 9112 §3.2.2: absolute-form MUST be accepted");
	EXPECT_STRm("http://example.com/x", req.path, "path");
}

/* RFC 9112 §2.2: "a server ... SHOULD ignore at least one empty line
 * (CRLF) received prior to the request-line" */
TEST(accept_leading_empty_line)
{
	struct http_request req;
	EXPECT_EQm(0, parse("\r\nGET / HTTP/1.1\r\nHost: h\r\n\r\n", &req),
	           "RFC 9112 §2.2: SHOULD ignore one leading CRLF");
	EXPECT_STRm("GET", req.method,
	            "RFC 9112 §2.2: CRLF ignored, not glued to method");
}

/* ---- conformance: invalid messages a server MUST (or SHOULD) reject ---- */

/* RFC 9112 §5.1: "A server MUST reject, with a response status code of
 * 400, any received request message that contains whitespace between a
 * header field name and colon." */
TEST(reject_ws_before_colon)
{
	struct http_request req;
	EXPECT_EQm(-1, parse("GET / HTTP/1.1\r\nHost : h\r\n\r\n", &req),
	           "RFC 9112 §5.1: MUST reject whitespace before colon");
}

/* RFC 9112 §3.2.6: "A server MUST respond with a 400 ... to any
 * HTTP/1.1 request message that lacks a Host header field" */
TEST(reject_missing_host)
{
	struct http_request req;
	EXPECT_EQm(-1, parse("GET / HTTP/1.1\r\nAccept: */*\r\n\r\n", &req),
	           "RFC 9112 §3.2.6: MUST reject missing Host");
}

/* RFC 9112 §3.2.6: "... and to any request message that contains more
 * than one Host header field line" */
TEST(reject_duplicate_host)
{
	struct http_request req;
	EXPECT_EQm(-1, parse("GET / HTTP/1.1\r\nHost: a\r\nHost: b\r\n\r\n", &req),
	           "RFC 9112 §3.2.6: MUST reject duplicate Host");
}

/* RFC 9112 §5.2: "A server that receives an obs-fold in a request
 * message ... MUST either reject the message by sending a 400 ... or
 * replace each received obs-fold with one or more SP octets" */
TEST(reject_obs_fold)
{
	struct http_request req;
	EXPECT_EQm(-1, parse("GET / HTTP/1.1\r\nX: a\r\n b\r\nHost: h\r\n\r\n", &req),
	           "RFC 9112 §5.2: MUST reject (or unfold) obs-fold");
}

/* RFC 9112 §2.2: "A recipient of such a bare CR MUST consider that
 * element to be invalid or replace each bare CR with SP" */
TEST(reject_bare_cr_in_field_value)
{
	struct http_request req;
	EXPECT_EQm(-1, parse("GET / HTTP/1.1\r\nX: a\rb\r\nHost: h\r\n\r\n", &req),
	           "RFC 9112 §2.2: bare CR in value MUST be rejected");
}

TEST(reject_bare_cr_in_request_target)
{
	struct http_request req;
	EXPECT_EQm(-1, parse("GET /\r/x HTTP/1.1\r\nHost: h\r\n\r\n", &req),
	           "RFC 9112 §2.2: bare CR in target MUST be rejected");
}

/* RFC 9112 §2.2: "A sender MUST NOT send whitespace between the
 * start-line and the first header field" — receiver MUST reject or
 * consume the whitespace-preceded line (smuggling vector). */
TEST(reject_ws_preceded_line)
{
	struct http_request req;
	EXPECT_EQm(-1, parse("GET / HTTP/1.1\r\n X: v\r\nHost: h\r\n\r\n", &req),
	           "RFC 9112 §2.2: MUST reject whitespace-preceded line");
}

/* RFC 9112 §5: field-line = field-name ":" OWS field-value OWS — a
 * line without a colon matches no grammar rule and is invalid framing.
 * Known bug: jhttp's memchr crosses the line and merges it into the
 * next header's key. */
TEST(reject_field_line_without_colon)
{
	struct http_request req;
	EXPECT_EQm(-1, parse("GET / HTTP/1.1\r\ngarbage line\r\nHost: h\r\n\r\n", &req),
	           "RFC 9112 §5: field-line without colon");
}

/* RFC 9110 §5.5: "a recipient of CR, LF, or NUL within a field value
 * MUST either reject the message or replace each of those characters
 * with SP" */
TEST(reject_nul_in_field_value)
{
	struct http_request req;
	static const char raw[] =
	    "GET / HTTP/1.1\r\nX: a\0b\r\nHost: h\r\n\r\n";
	EXPECT_EQm(-1, parse(raw, &req),
	           "RFC 9110 §5.5: NUL in value MUST be rejected");
}

/* RFC 9110 §5.1: field-name = token; §5.6.2: tchar = "!" / "#" / "$" /
 * "%" / "&" / "'" / "*" / "+" / "-" / "." / "^" / "_" / "`" / "|" /
 * "~" / DIGIT / ALPHA — '{', '}' are not tchar. */
TEST(reject_non_token_field_name)
{
	struct http_request req;
	EXPECT_EQm(-1, parse("GET / HTTP/1.1\r\n{bad}: v\r\nHost: h\r\n\r\n", &req),
	           "RFC 9110 §5.6.2: field-name must be token");
}

/* RFC 9110 §9.1: method = token; RFC 9112 §3.1: "recipient of an
 * invalid request-line SHOULD respond with a 400" — '@' is not tchar. */
TEST(reject_non_token_method)
{
	struct http_request req;
	EXPECT_EQm(-1, parse("G@T / HTTP/1.1\r\nHost: h\r\n\r\n", &req),
	           "RFC 9110 §9.1: method must be token (SHOULD 400)");
}

/* RFC 9112 §2.3: HTTP-version = "HTTP" "/" DIGIT "." DIGIT */
TEST(reject_malformed_version)
{
	struct http_request req;
	EXPECT_EQm(-1, parse("GET / FOO/9.9\r\nHost: h\r\n\r\n", &req),
	           "RFC 9112 §2.3: version is HTTP/DIGIT.DIGIT");
}

/* RFC 9112 §3.2: "No whitespace is allowed in the request-target" */
TEST(reject_ws_in_request_target)
{
	struct http_request req;
	EXPECT_EQm(-1, parse("GET /a /b HTTP/1.1\r\nHost: h\r\n\r\n", &req),
	           "RFC 9112 §3.2: whitespace in request-target");
}

/* RFC 9112 §3.2: request-target must match origin-form, absolute-form,
 * authority-form (CONNECT only) or asterisk-form (OPTIONS only). */
TEST(reject_invalid_target_form)
{
	struct http_request req;
	EXPECT_EQm(-1, parse("GET badtarget HTTP/1.1\r\nHost: h\r\n\r\n", &req),
	           "RFC 9112 §3.2: target matches no allowed form");
}

/* RFC 9112 §6.3 rule 5: "message framing is invalid and the recipient
 * MUST treat it as an unrecoverable error ... the server MUST respond
 * with a 400" — Content-Length must parse as a decimal. */
TEST(reject_invalid_content_length)
{
	struct http_request req;
	EXPECT_EQm(-1, parse("POST / HTTP/1.1\r\nHost: h\r\n"
	                     "Content-Length: abc\r\n\r\n", &req),
	           "RFC 9112 §6.3: non-decimal Content-Length");
}

/* RFC 9112 §6.3: multiple Content-Length values that differ make the
 * framing invalid (values must all be identical) — MUST reject. */
TEST(reject_conflicting_content_length)
{
	struct http_request req;
	EXPECT_EQm(-1, parse("POST / HTTP/1.1\r\nHost: h\r\n"
	                     "Content-Length: 5\r\n"
	                     "Content-Length: 6\r\n\r\n", &req),
	           "RFC 9112 §6.3: differing Content-Length values");
}

int main(void)
{
	RUN(request_line);
	RUN(path_with_query);
	RUN(single_header);
	RUN(multiple_headers);
	RUN(header_count_max);
	RUN(header_count_overflow);
	RUN(bare_lf_line_endings);
	RUN(empty_input);
	RUN(body_starts_after_blank_line);
	RUN(incomplete_is_rejected);

	RUN(accept_no_ows_after_colon);
	RUN(accept_htab_after_colon);
	RUN(accept_and_strip_surrounding_ows);
	RUN(accept_empty_field_value);
	RUN(accept_asterisk_form);
	RUN(accept_absolute_form);
	RUN(accept_leading_empty_line);

	RUN(reject_ws_before_colon);
	RUN(reject_missing_host);
	RUN(reject_duplicate_host);
	RUN(reject_obs_fold);
	RUN(reject_bare_cr_in_field_value);
	RUN(reject_bare_cr_in_request_target);
	RUN(reject_ws_preceded_line);
	RUN(reject_field_line_without_colon);
	RUN(reject_nul_in_field_value);
	RUN(reject_non_token_field_name);
	RUN(reject_non_token_method);
	RUN(reject_malformed_version);
	RUN(reject_ws_in_request_target);
	RUN(reject_invalid_target_form);
	RUN(reject_invalid_content_length);
	RUN(reject_conflicting_content_length);

	printf("%d passed, %d failed\n", test_pass, test_fail);
	free(cur);
	return test_fail != 0;
}
