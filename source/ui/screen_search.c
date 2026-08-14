#include "screen_search.h"

#include <stdio.h>
#include <string.h>

#include "thumbs.h"
#include "ui.h"

#define BOT_W    320.0f
#define BOT_H    240.0f
#define HEADER_H 30.0f
#define ROWS_TOP (HEADER_H + 4.0f)
#define ROW_H    44.0f
#define PAD_X    16.0f
#define THUMB    32.0f
#define THUMB_GAP 10.0f
#define IND_X    314.0f
#define IND_W    3.0f

#define CLR_HEADER   C2D_Color32(0x11, 0x11, 0x11, 0xFF)
#define CLR_NAME     C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF)
#define CLR_SUB      C2D_Color32(0x8A, 0x8A, 0x8A, 0xFF)
#define CLR_GREEN    C2D_Color32(0x1D, 0xB9, 0x54, 0xFF)
#define CLR_DIM      C2D_Color32(0x60, 0x60, 0x60, 0xFF)
#define CLR_NOW_BG   C2D_Color32(0x10, 0x16, 0x0F, 0xFF)
#define CLR_ROW_PR   C2D_Color32(0x24, 0x24, 0x24, 0xFF)
#define CLR_THUMB_BG C2D_Color32(0x22, 0x22, 0x2A, 0xFF)
#define CLR_ERROR    C2D_Color32(0xFF, 0x6B, 0x5B, 0xFF)
#define CLR_IND_TRK  C2D_Color32(0x26, 0x26, 0x26, 0xFF)
#define CLR_IND_THMB C2D_Color32(0x7A, 0x7A, 0x7A, 0xFF)

float screen_search_max_scroll(int count)
{
	const float max = (float)count * ROW_H - (BOT_H - ROWS_TOP);
	return max > 0.0f ? max : 0.0f;
}

static void draw_row(const screen_search_args *a, const track_item *item,
                     float y, int id)
{
	if (y >= BOT_H || y + ROW_H <= HEADER_H)
		return;

	const bool current = a->current_track_uri && a->current_track_uri[0] &&
	                     strcmp(item->uri, a->current_track_uri) == 0;

	if (current)
		C2D_DrawRectSolid(0.0f, y, 0.0f, BOT_W, ROW_H, CLR_NOW_BG);
	else if (a->pressed_id == id)
		C2D_DrawRectSolid(0.0f, y, 0.0f, BOT_W, ROW_H, CLR_ROW_PR);
	if (current)
		C2D_DrawRectSolid(0.0f, y, 0.0f, 3.0f, ROW_H, CLR_GREEN);

	const float thumb_y = y + (ROW_H - THUMB) / 2.0f;
	const C2D_Image *art = thumbs_get(item->art_url);
	if (art) {
		const float sx = THUMB / (float)art->subtex->width;
		const float sy = THUMB / (float)art->subtex->height;
		C2D_DrawImageAt(*art, PAD_X, thumb_y, 0.0f, NULL, sx, sy);
	} else {
		C2D_DrawRectSolid(PAD_X, thumb_y, 0.0f, THUMB, THUMB, CLR_THUMB_BG);
	}

	const float tx = PAD_X + THUMB + THUMB_GAP;
	const float tw = BOT_W - tx - 12.0f;
	const float name_h = ui_px(TY_ROW_NAME);
	const float sub_h = ui_px(TY_ROW_SUB);
	const float top = y + (ROW_H - name_h - sub_h - 2.0f) / 2.0f;
	const u32 name_clr = !item->playable ? CLR_DIM : current ? CLR_GREEN
	                                                          : CLR_NAME;
	ui_text(a->buf, item->name[0] ? item->name : "Unknown", tx,
	        ui_baseline(top, TY_ROW_NAME), TY_ROW_NAME, tw, name_clr);
	ui_text(a->buf, item->artist, tx,
	        ui_baseline(top + name_h + 2.0f, TY_ROW_SUB), TY_ROW_SUB, tw,
	        item->playable ? CLR_SUB : CLR_DIM);

	if (item->playable)
		tb_add(a->tb, 0.0f, y < ROWS_TOP ? ROWS_TOP : y, BOT_W,
		       (y + ROW_H > BOT_H ? BOT_H : y + ROW_H) -
		           (y < ROWS_TOP ? ROWS_TOP : y),
		       id);
}

void screen_search_draw(const screen_search_args *a)
{
	const int count = a->results ? a->results->count : 0;

	if (count > 0) {
		float y = ROWS_TOP - a->scroll;
		for (int i = 0; i < count; i++) {
			draw_row(a, &a->results->items[i], y, SEARCH_ROW0 + i);
			y += ROW_H;
		}

		const float max = screen_search_max_scroll(count);
		if (max > 0.0f) {
			C2D_DrawRectSolid(IND_X, ROWS_TOP, 0.0f, IND_W, BOT_H - ROWS_TOP,
			                  CLR_IND_TRK);
			float th = (BOT_H - ROWS_TOP) * (BOT_H - ROWS_TOP) /
			           ((float)count * ROW_H);
			if (th < 20.0f)
				th = 20.0f;
			const float ty =
			    ROWS_TOP + (BOT_H - ROWS_TOP - th) * (a->scroll / max);
			C2D_DrawRectSolid(IND_X, ty, 0.0f, IND_W, th, CLR_IND_THMB);
		}
	} else {
		const char *msg = a->loading      ? "Searching..."
		                  : a->error && a->error[0] && a->results &&
		                            a->results->query[0]
		                      ? a->error
		                  : a->results && a->results->query[0] ? "No results"
		                                                       : "Search Spotify";
		ui_text(a->buf, msg, PAD_X, ui_baseline(104.0f, TY_ROW_NAME),
		        TY_ROW_NAME, BOT_W - 2.0f * PAD_X,
		        a->error && a->error[0] ? CLR_ERROR : CLR_SUB);
		if (!a->loading)
			ui_text(a->buf, "Tap the query above to type a search.", PAD_X,
			        ui_baseline(128.0f, TY_ROW_SUB), TY_ROW_SUB,
			        BOT_W - 2.0f * PAD_X, CLR_DIM);
	}

	/* Header: back, the query (tap to edit), and a search glyph. */
	C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, BOT_W, HEADER_H, CLR_HEADER);
	const bool back_pressed = a->pressed_id == SEARCH_BTN_BACK;
	const u32  back_clr = back_pressed ? CLR_GREEN : CLR_NAME;
	C2D_DrawTriangle(PAD_X, 15.0f, back_clr, PAD_X + 7.0f, 10.0f, back_clr,
	                 PAD_X + 7.0f, 20.0f, back_clr, 0.0f);
	tb_add(a->tb, 0.0f, 0.0f, 34.0f, HEADER_H, SEARCH_BTN_BACK);

	const bool edit_pressed = a->pressed_id == SEARCH_BTN_EDIT;
	char label[80];
	if (a->results && a->results->query[0])
		snprintf(label, sizeof label, "%s", a->results->query);
	else
		snprintf(label, sizeof label, "Search...");
	ui_text(a->buf, label, 40.0f,
	        ui_baseline((HEADER_H - ui_px(TY_ROW_NAME)) / 2.0f, TY_ROW_NAME),
	        TY_ROW_NAME, BOT_W - 40.0f - 20.0f,
	        edit_pressed ? CLR_GREEN : CLR_NAME);
	tb_add(a->tb, 34.0f, 0.0f, BOT_W - 34.0f, HEADER_H, SEARCH_BTN_EDIT);
}
