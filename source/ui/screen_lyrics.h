#pragma once

#include <citro2d.h>

#include "../spotify/lyrics.h"
#include "touch.h"

enum {
	LYRICS_BTN_BACK = 900,
	LYRICS_BTN_RETRY,
	LYRICS_BTN_3D,
	LYRICS_ROW0 = 950, /* per-line tap-to-seek: LYRICS_ROW0 + line index */
};

typedef struct {
	C2D_TextBuf       buf;
	touch_builder    *tb;
	const lyrics_doc *doc;
	const C2D_Image  *cover;      /* album art for the blurred backdrop, or NULL */
	const char       *track;      /* header title; falls back to doc->track */
	const char       *back_label; /* left header label, e.g. "Player" */
	const char       *status;     /* message when there are no lines to show */
	bool              loading;
	bool              error;      /* draw a retry affordance */
	bool              show3d;     /* the top-screen 3D view is active */
	int               highlight;  /* active line index, or -1 */
	float             scroll;
	int               pressed_id;
} screen_lyrics_args;

void screen_lyrics_draw(const screen_lyrics_args *a);

/* Scroll geometry, so main.c can auto-follow the current line and clamp the
 * user's drag against the same numbers the draw uses. */
float screen_lyrics_max_scroll(int count);
float screen_lyrics_center_scroll(int count, int index);

/* Index of the line nearest the vertical centre of the body at a given scroll,
 * used to feature a line in the 3D view when lyrics are unsynced. */
int screen_lyrics_scroll_center_index(float scroll);
