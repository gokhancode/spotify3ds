#include "screen_lyrics.h"

#include <stdio.h>
#include <string.h>

#include "ui.h"

#define BOT_W       320.0f
#define BOT_H       240.0f
#define HEADER_H    30.0f
#define CONTENT_TOP (HEADER_H + 4.0f)
#define VIEWPORT_H  (BOT_H - CONTENT_TOP)
#define LINE_H      26.0f
#define PAD_X       16.0f
#define IND_X       314.0f
#define IND_W       3.0f

#define CLR_HEADER C2D_Color32(0x11, 0x11, 0x11, 0xFF)
#define CLR_NAME   C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF)
#define CLR_ACTIVE C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF)
#define CLR_PAST   C2D_Color32(0x59, 0x59, 0x59, 0xFF)
#define CLR_FUTURE C2D_Color32(0x9A, 0x9A, 0x9A, 0xFF)
#define CLR_PLAIN  C2D_Color32(0xC2, 0xC2, 0xC2, 0xFF)
#define CLR_SUB    C2D_Color32(0x8A, 0x8A, 0x8A, 0xFF)
#define CLR_GREEN  C2D_Color32(0x1D, 0xB9, 0x54, 0xFF)
#define CLR_GREEN_PRESS C2D_Color32(0x28, 0xD8, 0x68, 0xFF)
#define CLR_ACTION C2D_Color32(0x08, 0x08, 0x08, 0xFF)
#define CLR_ERROR  C2D_Color32(0xFF, 0x6B, 0x5B, 0xFF)
#define CLR_IND_TRK  C2D_Color32(0x26, 0x26, 0x26, 0xFF)
#define CLR_IND_THMB C2D_Color32(0x7A, 0x7A, 0x7A, 0xFF)

float screen_lyrics_max_scroll(int count)
{
	const float max = (float)count * LINE_H - VIEWPORT_H;
	return max > 0.0f ? max : 0.0f;
}

/* Scroll value that puts line `index` at the vertical centre of the body. */
float screen_lyrics_center_scroll(int count, int index)
{
	if (index < 0)
		return 0.0f;
	float scroll = (float)index * LINE_H + LINE_H / 2.0f - VIEWPORT_H / 2.0f;
	const float max = screen_lyrics_max_scroll(count);
	if (scroll < 0.0f)
		scroll = 0.0f;
	if (scroll > max)
		scroll = max;
	return scroll;
}

int screen_lyrics_scroll_center_index(float scroll)
{
	const int idx = (int)((scroll + VIEWPORT_H / 2.0f) / LINE_H);
	return idx < 0 ? 0 : idx;
}

static void rounded_rect(float x, float y, float w, float h, float r, u32 clr)
{
	C2D_DrawRectSolid(x + r, y, 0.0f, w - 2.0f * r, h, clr);
	C2D_DrawRectSolid(x, y + r, 0.0f, w, h - 2.0f * r, clr);
	ui_disc(x + r, y + r, r, clr);
	ui_disc(x + w - r, y + r, r, clr);
	ui_disc(x + r, y + h - r, r, clr);
	ui_disc(x + w - r, y + h - r, r, clr);
}

static void draw_lines(const screen_lyrics_args *a)
{
	const lyrics_doc *d = a->doc;
	const float role_h = ui_px(TY_ARTIST);
	const float maxw = BOT_W - 2.0f * PAD_X;

	for (int i = 0; i < d->count; i++) {
		const float top = CONTENT_TOP - a->scroll + (float)i * LINE_H;
		if (top + LINE_H <= CONTENT_TOP || top >= BOT_H)
			continue; /* fully outside the body */

		u32 clr;
		if (!d->synced) {
			clr = CLR_PLAIN;
		} else if (i == a->highlight) {
			clr = CLR_ACTIVE;
		} else if (a->highlight >= 0 && i < a->highlight) {
			clr = CLR_PAST;
		} else {
			clr = CLR_FUTURE;
		}

		if (d->synced && i == a->highlight)
			C2D_DrawRectSolid(0.0f, top + 3.0f, 0.0f, 3.0f, LINE_H - 6.0f,
			                  CLR_GREEN);

		const char *text = d->lines[i].text;
		if (!text[0])
			continue; /* blank spacer line: keep its slot, draw nothing */
		const float baseline = ui_baseline(top + (LINE_H - role_h) / 2.0f,
		                                    TY_ARTIST);
		ui_text(a->buf, text, PAD_X, baseline, TY_ARTIST, maxw, clr);
	}

	/* Scroll indicator, matching the tracks view. */
	const float max = screen_lyrics_max_scroll(d->count);
	if (max > 0.0f) {
		C2D_DrawRectSolid(IND_X, CONTENT_TOP, 0.0f, IND_W, VIEWPORT_H,
		                  CLR_IND_TRK);
		float th = VIEWPORT_H * VIEWPORT_H / ((float)d->count * LINE_H);
		if (th < 20.0f)
			th = 20.0f;
		const float ty = CONTENT_TOP + (VIEWPORT_H - th) * (a->scroll / max);
		C2D_DrawRectSolid(IND_X, ty, 0.0f, IND_W, th, CLR_IND_THMB);
	}
}

static void draw_status(const screen_lyrics_args *a)
{
	const char *message = a->loading ? "Loading lyrics..."
	                      : a->status && a->status[0] ? a->status
	                                                   : "No lyrics";
	const float maxw = BOT_W - 2.0f * PAD_X;
	ui_text(a->buf, message, PAD_X, ui_baseline(108.0f, TY_ROW_NAME),
	        TY_ROW_NAME, maxw, a->error ? CLR_ERROR : CLR_SUB);

	if (a->error) {
		const bool pressed = a->pressed_id == LYRICS_BTN_RETRY;
		rounded_rect(116.0f, 138.0f, 88.0f, 34.0f, 7.0f,
		             pressed ? CLR_GREEN_PRESS : CLR_GREEN);
		ui_text(a->buf, "RETRY  X", 128.0f,
		        ui_baseline(138.0f + (34.0f - ui_px(TY_ROW_NAME)) / 2.0f,
		                    TY_ROW_NAME),
		        TY_ROW_NAME, 70.0f, CLR_ACTION);
		tb_add(a->tb, 116.0f, 138.0f, 88.0f, 34.0f, LYRICS_BTN_RETRY);
	}
}

void screen_lyrics_draw(const screen_lyrics_args *a)
{
	if (a->doc && a->doc->count > 0)
		draw_lines(a);
	else
		draw_status(a);

	/* Fixed header on top, so scrolling lines slide under it. */
	C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, BOT_W, HEADER_H, CLR_HEADER);

	const bool back_pressed = a->pressed_id == LYRICS_BTN_BACK;
	const u32  back_clr = back_pressed ? CLR_GREEN : CLR_NAME;
	C2D_DrawTriangle(PAD_X, 15.0f, back_clr, PAD_X + 7.0f, 10.0f, back_clr,
	                 PAD_X + 7.0f, 20.0f, back_clr, 0.0f);
	ui_text(a->buf, a->back_label ? a->back_label : "Player", PAD_X + 17.0f,
	        ui_baseline((HEADER_H - ui_px(TY_ROW_NAME)) / 2.0f, TY_ROW_NAME),
	        TY_ROW_NAME, 70.0f, back_clr);
	tb_add(a->tb, 0.0f, 0.0f, 96.0f, HEADER_H, LYRICS_BTN_BACK);

	const char *title = a->track && a->track[0] ? a->track
	                    : a->doc && a->doc->track[0] ? a->doc->track
	                                                 : "Lyrics";
	const float tw = ui_text_width(a->buf, title, TY_ROW_NAME);
	float       tx = 96.0f + (186.0f - tw) / 2.0f; /* centred in the 96..282 gap */
	if (tx < 100.0f)
		tx = 100.0f;
	ui_text(a->buf, title, tx,
	        ui_baseline((HEADER_H - ui_px(TY_ROW_NAME)) / 2.0f, TY_ROW_NAME),
	        TY_ROW_NAME, 282.0f - tx, CLR_NAME);

	/* 3D toggle pill: sends the lyrics to the top screen as a hovering stack.
	 * Also bound to Y in main.c; this is the touch + on/off indicator. */
	const bool pill_pressed = a->pressed_id == LYRICS_BTN_3D;
	const u32  pill_bg = a->show3d ? (pill_pressed ? CLR_GREEN_PRESS : CLR_GREEN)
	                    : pill_pressed ? C2D_Color32(0x33, 0x33, 0x33, 0xFF)
	                                   : C2D_Color32(0x24, 0x24, 0x24, 0xFF);
	rounded_rect(286.0f, 6.0f, 28.0f, 18.0f, 6.0f, pill_bg);
	const float pill_tw = ui_text_width(a->buf, "3D", TY_MICRO);
	ui_text(a->buf, "3D", 286.0f + (28.0f - pill_tw) / 2.0f,
	        ui_baseline(6.0f + (18.0f - ui_px(TY_MICRO)) / 2.0f, TY_MICRO),
	        TY_MICRO, 28.0f, a->show3d ? CLR_ACTION : CLR_NAME);
	tb_add(a->tb, 278.0f, 0.0f, 42.0f, HEADER_H, LYRICS_BTN_3D);
}
