#pragma once

#include <stdbool.h>

/* Synced-and-plain lyrics from lrclib.net.
 *
 * The official Spotify Web API has no lyrics endpoint, so this app's Spotify
 * token is useless here. lrclib.net is an open, keyless community lyrics API
 * that returns both plain text and time-synced LRC, keyed by track/artist and
 * (optionally) duration. We prefer the synced form so the current line can be
 * highlighted against the live progress_ms, exactly like the desktop app.
 *
 * The LRC parse is a straight port of the reference implementation in
 * Projects/CDLyrics (LRCParser.swift / SyncedLyrics.swift) and its Python
 * original (cd_lyrics/lyrics/parser.py, models.py). */

#define LYRICS_MAX_LINES 200
#define LYRICS_TEXT_MAX  192

typedef struct {
	long time_ms;              /* -1 for an unsynced (plain-text) line */
	char text[LYRICS_TEXT_MAX];
} lyric_line;

typedef enum {
	LYRICS_OK = 0,       /* lines populated (synced or plain) */
	LYRICS_INSTRUMENTAL, /* lrclib flagged the track instrumental */
	LYRICS_NONE,         /* no match found */
	LYRICS_ERR,          /* transport/parse failure; see err */
} lyrics_result;

typedef struct {
	lyric_line lines[LYRICS_MAX_LINES];
	int        count;
	bool       synced;              /* timestamps present -> can auto-follow */
	char       track[LYRICS_TEXT_MAX];
	char       artist[LYRICS_TEXT_MAX];
} lyrics_doc;

/* Fetch lyrics for one track. Tries the exact /api/get match first, then falls
 * back to the fuzzier /api/search. duration_ms is the track length (0 if
 * unknown). Blocking network + parse: call from the worker thread, never the
 * render loop. out is fully overwritten on LYRICS_OK. */
lyrics_result lyrics_fetch(const char *track, const char *artist,
                           const char *album, long duration_ms, lyrics_doc *out,
                           char *err, int errlen);

/* Parse raw LRC text into out (out->count and out->synced=true are set; track
 * and artist are left untouched). Exposed so the parse can be exercised without
 * the network. */
void lyrics_parse_lrc(const char *lrc, lyrics_doc *out);

/* Index of the line active at elapsed_ms via binary search, or -1 before the
 * first timestamp / when the doc is empty or unsynced. */
int lyrics_index_at(const lyrics_doc *d, long elapsed_ms);
