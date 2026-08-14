#pragma once

#include <citro2d.h>

#include "../spotify/search.h"
#include "touch.h"

enum {
	SEARCH_BTN_BACK = 1100,
	SEARCH_BTN_EDIT, /* reopen the keyboard */
	SEARCH_ROW0 = 1110, /* + result index */
};

typedef struct {
	C2D_TextBuf           buf;
	touch_builder        *tb;
	const search_results *results;
	const char           *current_track_uri;
	const char           *error;
	bool                  loading;
	float                 scroll;
	int                   pressed_id;
} screen_search_args;

void  screen_search_draw(const screen_search_args *a);
float screen_search_max_scroll(int count);
