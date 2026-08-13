#pragma once

#include <citro2d.h>

#include "../spotify/lyrics.h"

/* Top-screen "hovering" lyrics.
 *
 * The current line floats in the foreground while the previous/next lines
 * recede into the background. On a real 3DS this is genuine stereoscopy: the
 * caller renders the scene once per eye and the horizontal parallax here makes
 * the hero line pop out of the panel. On a 2DS (no stereo hardware) the slider
 * reads 0, parallax collapses to nothing, and the size/brightness/shadow
 * gradient still sells the depth as a flat 2.5D stack. */

typedef struct {
	C2D_TextBuf       buf;
	const lyrics_doc *doc;
	int               center;       /* line to feature in the foreground */
	bool              active;       /* center is the live line (bright) */
	float             depth;        /* osGet3DSliderState(), 0..1 */
	float             eye;          /* -1.0 left target, +1.0 right target */
	unsigned          animation_ms; /* for the gentle hover bob */
} screen_lyrics3d_args;

void screen_lyrics3d_draw(const screen_lyrics3d_args *a);
