#pragma once

#include "player.h"
#include "tracks.h" /* track_item */

/* Spotify track search (GET /v1/search?type=track). Results reuse track_item so
 * the row renderer and artwork cache are shared with the collection views. */

#define SEARCH_MAX 24

typedef struct {
	track_item items[SEARCH_MAX];
	int        count;
	char       query[64];
} search_results;

/* Blocking search. Call from the worker thread. */
player_result search_tracks(const char *query, search_results *out, char *err,
                            int errlen);
