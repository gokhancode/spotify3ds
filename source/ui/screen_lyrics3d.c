#include "screen_lyrics3d.h"

#include <math.h>

#include "ui.h"

#define TOP_W    400.0f
#define TOP_H    240.0f
#define CENTER_Y 116.0f
#define ROW_STEP 50.0f
#define PAD_X    16.0f

/* Per-eye horizontal shift, in pixels, for a line one depth-unit in front of
 * the screen plane at full slider. Deliberately small: too much separation is
 * what reads as ghosting / "leaking" rather than depth. */
#define MAX_SHIFT 3.0f

static int iabs(int v)
{
	return v < 0 ? -v : v;
}

/* Draw a centred, parallaxed line of text with a soft shadow, at vertical
 * centre cy. Used for both the hero (twice, for its two wrapped rows) and the
 * single-line neighbours. */
static void draw_text_line(const screen_lyrics3d_args *a, const char *text,
                           float cy, type_role role, float depthval, u32 clr)
{
	if (!text[0])
		return;
	const float maxw = TOP_W - 2.0f * PAD_X;
	const float w = ui_text_width(a->buf, text, role);
	float       x = w < maxw ? (TOP_W - w) / 2.0f : PAD_X;

	/* Crossed disparity in front of the panel, uncrossed behind it. */
	x -= a->eye * (depthval * MAX_SHIFT * a->depth);

	const float top = cy - ui_px(role) / 2.0f;
	ui_text(a->buf, text, x + 1.5f, ui_baseline(top + 1.5f, role), role, maxw,
	        C2D_Color32(0x00, 0x00, 0x00, 0xB0));
	ui_text(a->buf, text, x, ui_baseline(top, role), role, maxw, clr);
}

/* One stack row at offset `d` from the featured line (0 = hero). */
static void draw_line(const screen_lyrics3d_args *a, int d)
{
	const int idx = a->center + d;
	if (idx < 0 || idx >= a->doc->count)
		return;
	const char *text = a->doc->lines[idx].text;
	if (!text[0])
		return;

	const int mag = iabs(d);
	/* Gentle, monotonic depth: hero eased forward, neighbours at the panel,
	 * the outer pair set just behind it. */
	const float depthval = mag == 0 ? 0.6f : mag == 1 ? 0.0f : -0.4f;

	u32 clr;
	if (mag == 0)
		clr = a->active ? C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF)
		                : C2D_Color32(0xDD, 0xDD, 0xDD, 0xFF);
	else if (mag == 1)
		clr = C2D_Color32(0xC8, 0xC8, 0xC8, 0xF0);
	else
		clr = C2D_Color32(0x9A, 0x9A, 0x9A, 0xA0);

	float cy = CENTER_Y + (float)d * ROW_STEP;

	if (mag == 0) {
		/* Hero: wrap to two rows, add a slow hover bob. */
		cy += sinf((float)a->animation_ms * 0.0028f) * 2.0f;
		char lines[2][128];
		const int   n = ui_wrap(text, TY_TITLE, TOP_W - 2.0f * PAD_X, lines, 2);
		const float lh = ui_px(TY_TITLE) + 4.0f;
		float       first_cy = cy - (float)(n - 1) * lh / 2.0f;
		for (int k = 0; k < n; k++)
			draw_text_line(a, lines[k], first_cy + (float)k * lh, TY_TITLE,
			               depthval, clr);
		return;
	}

	draw_text_line(a, text, cy, mag == 1 ? TY_ARTIST_L : TY_ARTIST, depthval,
	               clr);
}

void screen_lyrics3d_draw(const screen_lyrics3d_args *a)
{
	/* Dimmed cover fills the panel at the screen plane (no parallax), giving the
	 * floating lines a stable depth reference. */
	ui_backdrop(a->cover, TOP_W, TOP_H);

	if (!a->doc || a->doc->count <= 0)
		return;

	/* Back to front so the foreground line overlaps those behind it. */
	static const int order[] = {-2, 2, -1, 1, 0};
	for (unsigned i = 0; i < sizeof order / sizeof order[0]; i++)
		draw_line(a, order[i]);
}
