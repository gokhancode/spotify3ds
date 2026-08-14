#include "search.h"

#include <3ds.h>
#include <stdio.h>
#include <string.h>

#include "auth.h"
#include "json.h"
#include "../net/http.h"
#include "../testlog.h"

#define API_HOST "api.spotify.com"

/* Percent-encode a search query for the q= parameter. */
static void urlencode(const char *src, char *out, size_t outlen)
{
	static const char hex[] = "0123456789ABCDEF";
	size_t o = 0;
	for (size_t i = 0; src && src[i] && o + 4 < outlen; i++) {
		const unsigned char c = (unsigned char)src[i];
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		    (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
		    c == '~') {
			out[o++] = (char)c;
		} else {
			out[o++] = '%';
			out[o++] = hex[c >> 4];
			out[o++] = hex[c & 0x0F];
		}
	}
	out[o] = '\0';
}

/* Join up to four artist names into "A, B, C". Mirrors tracks.c. */
static void join_artists(const json_doc *d, const char *base, char *out,
                         int outlen)
{
	out[0] = '\0';
	for (int i = 0; i < 4; i++) {
		char path[128], name[96];
		snprintf(path, sizeof path, "%s.artists[%d].name", base, i);
		if (!json_doc_str(d, path, name, sizeof name))
			break;
		const size_t used = strlen(out);
		snprintf(out + used, (size_t)outlen - used, "%s%s", used ? ", " : "",
		         name);
	}
}

player_result search_tracks(const char *query, search_results *out, char *err,
                            int errlen)
{
	memset(out, 0, sizeof *out);
	snprintf(out->query, sizeof out->query, "%s", query ? query : "");
	if (!query || !query[0]) {
		snprintf(err, errlen, "empty query");
		return PLAYER_ERROR;
	}

	const char *token = auth_token(err, errlen);
	if (!token)
		return PLAYER_AUTH_FAILED;

	char eq[256];
	urlencode(query, eq, sizeof eq);
	char path[400];
	/* No market= : with a user token Spotify already scopes results to the
	 * account's country, and `market=from_token` is the likeliest 403 culprit. */
	snprintf(path, sizeof path, "/v1/search?q=%s&type=track&limit=%d", eq,
	         SEARCH_MAX);

	http_response r;
	const u64     t0 = osGetTime();
	if (!http_request(API_HOST, "GET", path, token, NULL, NULL, &r, err, errlen))
		return PLAYER_ERROR;

	if (r.status != 200 || !r.body || !r.body_len) {
		const int st = r.status;
		/* Surface Spotify's own error message so a 403/400 explains itself. */
		char msg[128] = "";
		if (r.body && r.body_len)
			json_get_str(r.body, r.body_len, "error.message", msg, sizeof msg);
		snprintf(err, errlen, "search http %d%s%s", st, msg[0] ? ": " : "",
		         msg);
		http_free(&r);
		return st == 401   ? PLAYER_AUTH_FAILED
		       : st == 403 ? PLAYER_FORBIDDEN
		                   : PLAYER_ERROR;
	}

	int       needed = 0;
	json_doc *d = json_doc_parse(r.body, r.body_len, &needed);
	if (!d) {
		snprintf(err, errlen, "search parse failed (%u bytes)",
		         (unsigned)r.body_len);
		http_free(&r);
		return PLAYER_ERROR;
	}

	int count = json_doc_array_size(d, "tracks.items");
	if (count < 0)
		count = 0;
	if (count > SEARCH_MAX)
		count = SEARCH_MAX;
	out->count = count;

	for (int i = 0; i < count; i++) {
		track_item *it = &out->items[i];
		char        base[32], field[64];
		snprintf(base, sizeof base, "tracks.items[%d]", i);
		it->kind = TRACK_ITEM_TRACK;

		snprintf(field, sizeof field, "%s.name", base);
		json_doc_str(d, field, it->name, sizeof it->name);
		snprintf(field, sizeof field, "%s.uri", base);
		json_doc_str(d, field, it->uri, sizeof it->uri);

		long v = 0;
		snprintf(field, sizeof field, "%s.duration_ms", base);
		if (json_doc_int(d, field, &v))
			it->duration_ms = v;

		bool b = false;
		snprintf(field, sizeof field, "%s.is_playable", base);
		it->playable = json_doc_bool(d, field, &b) ? b : 1;
		snprintf(field, sizeof field, "%s.explicit", base);
		if (json_doc_bool(d, field, &b))
			it->explicit_content = b;

		/* Small (index 2) cover for the row thumbnail, falling back to the
		 * largest if a track lacks the small size. */
		snprintf(field, sizeof field, "%s.album.images[2].url", base);
		if (!json_doc_str(d, field, it->art_url, sizeof it->art_url)) {
			snprintf(field, sizeof field, "%s.album.images[0].url", base);
			json_doc_str(d, field, it->art_url, sizeof it->art_url);
		}

		join_artists(d, base, it->artist, sizeof it->artist);
		if (!it->uri[0])
			it->playable = 0;
	}

	json_doc_free(d);
	tl_timing("search '%s' -> %d results (%llums)", query, out->count,
	          (unsigned long long)(osGetTime() - t0));
	http_free(&r);
	return PLAYER_OK;
}
