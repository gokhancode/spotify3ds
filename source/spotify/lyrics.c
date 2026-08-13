#include "lyrics.h"

#include <3ds.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"
#include "../net/http.h"
#include "../testlog.h"

#define LRCLIB_HOST "lrclib.net"

/* Percent-encode a query value, stopping after `cap` source bytes so a long
 * title cannot blow past http.c's request-line buffer. Only the RFC 3986
 * unreserved set is passed through untouched. */
static void urlencode_capped(const char *src, size_t cap, char *out,
                             size_t outlen)
{
	static const char hex[] = "0123456789ABCDEF";
	size_t o = 0;
	if (!src)
		src = "";
	for (size_t i = 0; src[i] && i < cap && o + 4 < outlen; i++) {
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

/* Parse one LRC tag at p (which must point at '['): [mm:ss.xx] or [mm:ss.xxx].
 * On success returns true, writes the millisecond value, and advances *end past
 * the closing ']'. Metadata tags like [ar:...] fail here (non-digit body). */
static bool parse_one_ts(const char *p, const char **end, long *out_ms)
{
	if (*p != '[')
		return false;
	const char *q = p + 1;

	int mm = 0, mdigits = 0;
	while (isdigit((unsigned char)*q) && mdigits < 3) {
		mm = mm * 10 + (*q - '0');
		q++;
		mdigits++;
	}
	if (mdigits < 1 || *q != ':')
		return false;
	q++;

	if (!isdigit((unsigned char)q[0]) || !isdigit((unsigned char)q[1]))
		return false;
	const int ss = (q[0] - '0') * 10 + (q[1] - '0');
	q += 2;
	if (*q != '.')
		return false;
	q++;

	int frac = 0, fdigits = 0;
	while (isdigit((unsigned char)*q) && fdigits < 3) {
		frac = frac * 10 + (*q - '0');
		q++;
		fdigits++;
	}
	if (fdigits < 2 || *q != ']')
		return false;
	q++;

	long ms = (long)mm * 60000 + (long)ss * 1000;
	ms += (fdigits == 2) ? frac * 10 : frac; /* centiseconds vs milliseconds */

	*out_ms = ms;
	*end = q;
	return true;
}

static int cmp_line_time(const void *a, const void *b)
{
	const lyric_line *la = a;
	const lyric_line *lb = b;
	if (la->time_ms < lb->time_ms)
		return -1;
	if (la->time_ms > lb->time_ms)
		return 1;
	return 0;
}

static void store_line(lyrics_doc *out, long time_ms, const char *start,
                       const char *stop)
{
	if (out->count >= LYRICS_MAX_LINES)
		return;
	int len = (int)(stop - start);
	if (len < 0)
		len = 0;
	if (len >= LYRICS_TEXT_MAX)
		len = LYRICS_TEXT_MAX - 1;
	lyric_line *ln = &out->lines[out->count++];
	ln->time_ms = time_ms;
	memcpy(ln->text, start, (size_t)len);
	ln->text[len] = '\0';
}

void lyrics_parse_lrc(const char *lrc, lyrics_doc *out)
{
	out->count = 0;
	out->synced = true;

	for (const char *p = lrc; *p && out->count < LYRICS_MAX_LINES;) {
		const char *eol = p;
		while (*eol && *eol != '\n')
			eol++;

		const char *s = p;
		while (s < eol && (*s == ' ' || *s == '\t'))
			s++;

		long times[12];
		int  ntimes = 0;
		while (s < eol && *s == '[' && ntimes < 12) {
			const char *tend;
			long        ms;
			if (parse_one_ts(s, &tend, &ms) && tend <= eol) {
				times[ntimes++] = ms;
				s = tend;
			} else {
				break;
			}
		}

		if (ntimes > 0) {
			/* Text is whatever follows the final timestamp on this line. */
			const char *tstart = s;
			const char *tstop  = eol;
			while (tstart < tstop && (*tstart == ' ' || *tstart == '\t'))
				tstart++;
			while (tstop > tstart &&
			       (tstop[-1] == '\r' || tstop[-1] == ' ' || tstop[-1] == '\t'))
				tstop--;
			for (int i = 0; i < ntimes; i++)
				store_line(out, times[i], tstart, tstop);
		}

		p = (*eol == '\n') ? eol + 1 : eol;
	}

	qsort(out->lines, (size_t)out->count, sizeof out->lines[0], cmp_line_time);
}

/* Plain (unsynced) fallback: one line per newline, timestamps set to -1. */
static void parse_plain(const char *plain, lyrics_doc *out)
{
	out->count = 0;
	out->synced = false;

	for (const char *p = plain; *p && out->count < LYRICS_MAX_LINES;) {
		const char *eol = p;
		while (*eol && *eol != '\n')
			eol++;
		const char *stop = eol;
		while (stop > p && stop[-1] == '\r')
			stop--;
		store_line(out, -1, p, stop);
		p = (*eol == '\n') ? eol + 1 : eol;
	}
}

int lyrics_index_at(const lyrics_doc *d, long elapsed_ms)
{
	if (!d || d->count <= 0 || !d->synced)
		return -1;
	if (elapsed_ms < d->lines[0].time_ms)
		return -1;

	int lo = 0, hi = d->count - 1;
	while (lo <= hi) {
		const int mid = (lo + hi) / 2;
		if (d->lines[mid].time_ms <= elapsed_ms)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return hi;
}

static void fill_meta(lyrics_doc *out, const char *track, const char *artist)
{
	snprintf(out->track, sizeof out->track, "%s", track ? track : "");
	snprintf(out->artist, sizeof out->artist, "%s", artist ? artist : "");
}

/* Read a non-empty string field `<base>field` from a parsed document. base is
 * "" for an object root or "[i]" for a search-result element. */
static bool get_field(const json_doc *d, const char *base, const char *field,
                      char *buf, size_t buflen)
{
	char path[64];
	snprintf(path, sizeof path, "%s%s%s", base, base[0] ? "." : "", field);
	if (json_doc_is_null(d, path))
		return false;
	if (!json_doc_str(d, path, buf, buflen))
		return false;
	return buf[0] != '\0';
}

/* Turn one lrclib object (at `base`) into out. Prefers synced lyrics, falls
 * back to plain. Returns true only when at least one line was produced. */
static bool doc_from_object(const json_doc *d, const char *base,
                            const char *track, const char *artist, char *buf,
                            size_t buflen, lyrics_doc *out)
{
	if (get_field(d, base, "syncedLyrics", buf, buflen)) {
		fill_meta(out, track, artist);
		lyrics_parse_lrc(buf, out);
		if (out->count > 0)
			return true;
	}
	if (get_field(d, base, "plainLyrics", buf, buflen)) {
		fill_meta(out, track, artist);
		parse_plain(buf, out);
		if (out->count > 0)
			return true;
	}
	return false;
}

lyrics_result lyrics_fetch(const char *track, const char *artist,
                           const char *album, long duration_ms, lyrics_doc *out,
                           char *err, int errlen)
{
	if (!track || !track[0] || !artist || !artist[0]) {
		snprintf(err, errlen, "no track/artist");
		return LYRICS_NONE;
	}

	char et[256], ea[256], eb[256];
	urlencode_capped(track, 80, et, sizeof et);
	urlencode_capped(artist, 64, ea, sizeof ea);
	urlencode_capped(album, 64, eb, sizeof eb);

	const u64 t0 = osGetTime();
	bool instrumental = false;

	/* --- provider 1: exact match ------------------------------------- */
	char path[900];
	int  n = snprintf(path, sizeof path,
	                  "/api/get?track_name=%s&artist_name=%s", et, ea);
	if (n < 0 || n >= (int)sizeof path)
		n = (int)sizeof path - 1;
	if (album && album[0])
		n += snprintf(path + n, sizeof path - (size_t)n, "&album_name=%s", eb);
	if (n < (int)sizeof path && duration_ms > 0)
		snprintf(path + n, sizeof path - (size_t)n, "&duration=%ld",
		         duration_ms / 1000);

	http_response r;
	if (http_request(LRCLIB_HOST, "GET", path, NULL, NULL, NULL, &r, err,
	                 errlen)) {
		if (r.status == 200 && r.body && r.body_len) {
			int       needed = 0;
			json_doc *d = json_doc_parse(r.body, r.body_len, &needed);
			if (d) {
				bool inst = false;
				if (json_doc_bool(d, "instrumental", &inst) && inst)
					instrumental = true;
				char *buf = malloc(r.body_len + 1);
				if (buf) {
					const bool got = doc_from_object(d, "", track, artist, buf,
					                                 r.body_len + 1, out);
					free(buf);
					if (got) {
						json_doc_free(d);
						http_free(&r);
						tl_timing("lyrics get ok: %d lines synced=%d %llums",
						          out->count, (int)out->synced,
						          (unsigned long long)(osGetTime() - t0));
						return LYRICS_OK;
					}
				}
				json_doc_free(d);
			}
		}
		http_free(&r);
	}

	if (instrumental) {
		tl_log("lyrics: instrumental %s - %s", track, artist);
		return LYRICS_INSTRUMENTAL;
	}

	/* --- provider 2: fuzzy search ------------------------------------ */
	char spath[900];
	snprintf(spath, sizeof spath, "/api/search?track_name=%s&artist_name=%s",
	         et, ea);

	http_response s;
	if (http_request(LRCLIB_HOST, "GET", spath, NULL, NULL, NULL, &s, err,
	                 errlen)) {
		if (s.status == 200 && s.body && s.body_len) {
			int       needed = 0;
			json_doc *d = json_doc_parse(s.body, s.body_len, &needed);
			if (d) {
				int count = json_doc_array_size(d, "");
				if (count > 20)
					count = 20; /* first matches are the best ranked */
				char *buf = malloc(s.body_len + 1);
				if (buf) {
					for (int i = 0; i < count; i++) {
						char base[16];
						snprintf(base, sizeof base, "[%d]", i);
						if (doc_from_object(d, base, track, artist, buf,
						                    s.body_len + 1, out)) {
							free(buf);
							json_doc_free(d);
							http_free(&s);
							tl_timing(
							    "lyrics search ok: %d lines synced=%d %llums",
							    out->count, (int)out->synced,
							    (unsigned long long)(osGetTime() - t0));
							return LYRICS_OK;
						}
					}
					free(buf);
				}
				json_doc_free(d);
			}
		}
		http_free(&s);
	}

	tl_log("lyrics: none for %s - %s", track, artist);
	snprintf(err, errlen, "no lyrics found");
	return LYRICS_NONE;
}
