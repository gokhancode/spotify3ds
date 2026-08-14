#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
	int    status;    /* HTTP status code, e.g. 200 / 204 / 401 */
	char  *body;      /* NUL-terminated; NULL when there is no body */
	size_t body_len;
	char   location[1024]; /* Location on a 3xx (CDN URLs are long), else empty */
} http_response;

/* Perform one HTTPS request and read the full response.
 *
 * method   "GET" / "PUT" / "POST"
 * path     e.g. "/v1/me/player/currently-playing"
 * bearer   access token, or NULL for no Authorization header
 * ctype    Content-Type for the body, or NULL
 * body     request body, or NULL
 *
 * Returns true if a well-formed response was read (any status code, including
 * 4xx/5xx). Returns false only on transport failure, with err filled in.
 *
 * Handles both Content-Length and chunked transfer-encoding: Spotify uses
 * chunked for JSON responses.
 *
 * Caller must http_free() the response.
 */
bool http_request(const char *host, const char *method, const char *path,
                  const char *bearer, const char *ctype, const char *body,
                  http_response *out, char *err, int errlen);

/* GET an absolute https URL, following up to max_redirects cross-host 3xx
 * redirects (as GitHub release downloads require). On success `out` holds the
 * final response, which the caller must http_free(). */
bool http_get_follow(const char *url, http_response *out, int max_redirects,
                     char *err, int errlen);

void http_free(http_response *r);
