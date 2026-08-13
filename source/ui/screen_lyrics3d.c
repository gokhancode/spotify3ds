#include "screen_lyrics3d.h"

#include <math.h>

#include "ui.h"

#define TOP_W    400.0f
#define CENTER_Y 118.0f
#define ROW_STEP 44.0f
#define PAD_X    14.0f

/* Per-eye horizontal shift, in pixels, for a line one depth-unit in front of
 * the screen plane at full slider. Kept modest so the effect is comfortable
 * rather than a headache. */
#define MAX_SHIFT 4.0f

static int iabs(int v)
{
	return v < 0 ? -v : v;
}

/* Draw one line at stack offset `d` from the featured line (0 = hero). Ordering
 * is caller's responsibility: draw outer rows first so the hero lands on top. */
static void draw_line(const screen_lyrics3d_args *a, int d)
{
	const int idx = a->center + d;
	if (idx < 0 || idx >= a->doc->count)
		return;
	const char *text = a->doc->lines[idx].text;
	if (!text[0])
		return; /* blank spacer line */

	const int mag = iabs(d);
	const type_role role = mag == 0   ? TY_TITLE
	                       : mag == 1  ? TY_ARTIST_L
	                                   : TY_ARTIST;

	/* Positive depth pops toward the viewer; neighbours sit behind the panel. */
	const float depthval = mag == 0 ? 1.0f : mag == 1 ? -0.15f : -0.6f;

	u32 clr;
	if (mag == 0)
		clr = a->active ? C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF)
		                : C2D_Color32(0xDD, 0xDD, 0xDD, 0xFF);
	else if (mag == 1)
		clr = C2D_Color32(0xB4, 0xB4, 0xB4, 0xD0);
	else
		clr = C2D_Color32(0x8C, 0x8C, 0x8C, 0x82);

	const float role_h = ui_px(role);
	float       center_y = CENTER_Y + (float)d * ROW_STEP;
	if (mag == 0) /* gentle hover bob on the hero line */
		center_y += sinf((float)a->animation_ms * 0.003f) * 2.5f;

	const float maxw = TOP_W - 2.0f * PAD_X;
	const float w = ui_text_width(a->buf, text, role);
	float       x = w < maxw ? (TOP_W - w) / 2.0f : PAD_X;

	const float sep = depthval * MAX_SHIFT * a->depth;
	x -= a->eye * sep; /* crossed disparity for the hero, uncrossed behind */

	const float top = center_y - role_h / 2.0f;
	if (mag == 0) {
		/* Drop shadow lifts the hero off the receding lines. */
		ui_text(a->buf, text, x + 2.0f, ui_baseline(top + 3.0f, role), role,
		        maxw, C2D_Color32(0x00, 0x00, 0x00, 0xB0));
	}
	ui_text(a->buf, text, x, ui_baseline(top, role), role, maxw, clr);
}

void screen_lyrics3d_draw(const screen_lyrics3d_args *a)
{
	if (!a->doc || a->doc->count <= 0)
		return;

	/* Back to front so the foreground line overlaps the ones behind it. */
	static const int order[] = {-2, 2, -1, 1, 0};
	for (unsigned i = 0; i < sizeof order / sizeof order[0]; i++)
		draw_line(a, order[i]);
}
