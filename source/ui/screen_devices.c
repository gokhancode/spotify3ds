#include "screen_devices.h"

#include <string.h>

#include "ui.h"

#define BOT_W    320.0f
#define BOT_H    240.0f
#define HEADER_H 30.0f
#define ROWS_TOP (HEADER_H + 4.0f)
#define ROW_H    44.0f
#define PAD_X    16.0f

#define CLR_HEADER  C2D_Color32(0x11, 0x11, 0x11, 0xFF)
#define CLR_NAME    C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF)
#define CLR_SUB     C2D_Color32(0x8A, 0x8A, 0x8A, 0xFF)
#define CLR_GREEN   C2D_Color32(0x1D, 0xB9, 0x54, 0xFF)
#define CLR_DIM     C2D_Color32(0x60, 0x60, 0x60, 0xFF)
#define CLR_ROW_TGT C2D_Color32(0x10, 0x16, 0x0F, 0xFF)
#define CLR_ROW_PR  C2D_Color32(0x24, 0x24, 0x24, 0xFF)
#define CLR_GREEN_PR C2D_Color32(0x28, 0xD8, 0x68, 0xFF)
#define CLR_ACTION  C2D_Color32(0x08, 0x08, 0x08, 0xFF)
#define CLR_ERROR   C2D_Color32(0xFF, 0x6B, 0x5B, 0xFF)
#define FOOTER_Y    206.0f
#define LIST_BOTTOM 178.0f /* rows stop here so they never hit the footer */

static void rounded_rect(float x, float y, float w, float h, float r, u32 clr)
{
	C2D_DrawRectSolid(x + r, y, 0.0f, w - 2.0f * r, h, clr);
	C2D_DrawRectSolid(x, y + r, 0.0f, w, h - 2.0f * r, clr);
	ui_disc(x + r, y + r, r, clr);
	ui_disc(x + w - r, y + r, r, clr);
	ui_disc(x + r, y + h - r, r, clr);
	ui_disc(x + w - r, y + h - r, r, clr);
}

static void draw_check(float cx, float cy, u32 clr)
{
	C2D_DrawLine(cx - 5.0f, cy, clr, cx - 1.0f, cy + 4.0f, clr, 2.0f, 0.0f);
	C2D_DrawLine(cx - 1.0f, cy + 4.0f, clr, cx + 5.0f, cy - 5.0f, clr, 2.0f,
	             0.0f);
}

static void draw_row(const screen_devices_args *a, int i, float y)
{
	const device_item *d = &a->devices->items[i];
	const bool active = a->active_id && a->active_id[0] &&
	                    strcmp(d->id, a->active_id) == 0;
	const bool target = a->target_id && a->target_id[0] &&
	                    strcmp(d->id, a->target_id) == 0;

	if (target)
		C2D_DrawRectSolid(0.0f, y, 0.0f, BOT_W, ROW_H, CLR_ROW_TGT);
	if (a->pressed_id == DEVICE_ROW0 + i)
		C2D_DrawRectSolid(0.0f, y, 0.0f, BOT_W, ROW_H, CLR_ROW_PR);
	if (target || active)
		C2D_DrawRectSolid(0.0f, y, 0.0f, 3.0f, ROW_H, CLR_GREEN);

	const u32 name_clr = d->is_restricted ? CLR_DIM : CLR_NAME;
	const float name_h = ui_px(TY_ROW_NAME);
	const float sub_h = ui_px(TY_ROW_SUB);
	const float top = y + (ROW_H - name_h - sub_h - 2.0f) / 2.0f;
	ui_text(a->buf, d->name[0] ? d->name : "Unknown device", PAD_X,
	        ui_baseline(top, TY_ROW_NAME), TY_ROW_NAME, BOT_W - PAD_X - 40.0f,
	        name_clr);

	char sub[96];
	if (d->is_restricted)
		snprintf(sub, sizeof sub, "%s \xC2\xB7 can't control", d->type);
	else if (active)
		snprintf(sub, sizeof sub, "%s \xC2\xB7 playing", d->type);
	else
		snprintf(sub, sizeof sub, "%s", d->type[0] ? d->type : "device");
	ui_text(a->buf, sub, PAD_X, ui_baseline(top + name_h + 2.0f, TY_ROW_SUB),
	        TY_ROW_SUB, BOT_W - PAD_X - 40.0f, active ? CLR_GREEN : CLR_SUB);

	if (target)
		draw_check(BOT_W - 22.0f, y + ROW_H / 2.0f, CLR_GREEN);

	if (!d->is_restricted)
		tb_add(a->tb, 0.0f, y, BOT_W, ROW_H, DEVICE_ROW0 + i);
}

/* Footer: the self-update button and its progress line. */
static void draw_footer(const screen_devices_args *a)
{
	const char *status = NULL;
	u32         sclr = CLR_SUB;
	switch (a->update_stage) {
		case UPDATE_DOWNLOADING: status = "Downloading update..."; break;
		case UPDATE_INSTALLING:  status = "Installing - do not close"; break;
		case UPDATE_DONE:
			status = "Updated! Exit and reopen.";
			sclr = CLR_GREEN;
			break;
		case UPDATE_FAILED:
			status = a->update_msg && a->update_msg[0] ? a->update_msg
			                                           : "Update failed";
			sclr = CLR_ERROR;
			break;
		default: break;
	}
	if (status)
		ui_text(a->buf, status, PAD_X, ui_baseline(FOOTER_Y - 15.0f, TY_ROW_SUB),
		        TY_ROW_SUB, BOT_W - 2.0f * PAD_X, sclr);

	const bool busy = a->update_stage == UPDATE_DOWNLOADING ||
	                  a->update_stage == UPDATE_INSTALLING;
	const bool  pr = a->pressed_id == DEVICE_BTN_UPDATE;
	const char *label = busy ? "Updating..."
	                    : a->update_stage == UPDATE_DONE ? "Done"
	                                                      : "Update app";
	const float lw = ui_text_width(a->buf, label, TY_ROW_NAME);
	const float w = lw + 28.0f;
	const float x = (BOT_W - w) / 2.0f;
	const float h = 26.0f;
	rounded_rect(x, FOOTER_Y, w, h, h / 2.0f,
	             busy ? CLR_ROW_PR : pr ? CLR_GREEN_PR : CLR_GREEN);
	ui_text(a->buf, label, x + 14.0f,
	        ui_baseline(FOOTER_Y + (h - ui_px(TY_ROW_NAME)) / 2.0f, TY_ROW_NAME),
	        TY_ROW_NAME, lw + 2.0f, busy ? CLR_SUB : CLR_ACTION);
	if (!busy)
		tb_add(a->tb, x - 6.0f, FOOTER_Y - 6.0f, w + 12.0f, h + 12.0f,
		       DEVICE_BTN_UPDATE);
}

void screen_devices_draw(const screen_devices_args *a)
{
	const int count = a->devices ? a->devices->count : 0;

	if (count > 0) {
		float y = ROWS_TOP;
		for (int i = 0; i < count && y < LIST_BOTTOM; i++) {
			draw_row(a, i, y);
			y += ROW_H;
		}
	} else {
		const char *msg = a->loading ? "Looking for devices..."
		                             : "No devices found";
		ui_text(a->buf, msg, PAD_X, ui_baseline(96.0f, TY_ROW_NAME),
		        TY_ROW_NAME, BOT_W - 2.0f * PAD_X, CLR_SUB);
		if (!a->loading)
			ui_text(a->buf, "Open Spotify on a PC or phone, then Refresh.",
			        PAD_X, ui_baseline(120.0f, TY_ROW_SUB), TY_ROW_SUB,
			        BOT_W - 2.0f * PAD_X, CLR_DIM);
	}

	draw_footer(a);

	/* Header on top. */
	C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, BOT_W, HEADER_H, CLR_HEADER);
	const bool back_pressed = a->pressed_id == DEVICE_BTN_BACK;
	const u32  back_clr = back_pressed ? CLR_GREEN : CLR_NAME;
	C2D_DrawTriangle(PAD_X, 15.0f, back_clr, PAD_X + 7.0f, 10.0f, back_clr,
	                 PAD_X + 7.0f, 20.0f, back_clr, 0.0f);
	ui_text(a->buf, "Player", PAD_X + 17.0f,
	        ui_baseline((HEADER_H - ui_px(TY_ROW_NAME)) / 2.0f, TY_ROW_NAME),
	        TY_ROW_NAME, 70.0f, back_clr);
	tb_add(a->tb, 0.0f, 0.0f, 96.0f, HEADER_H, DEVICE_BTN_BACK);

	ui_text(a->buf, "Play on", 104.0f,
	        ui_baseline((HEADER_H - ui_px(TY_ROW_NAME)) / 2.0f, TY_ROW_NAME),
	        TY_ROW_NAME, 120.0f, CLR_NAME);

	const bool refresh_pressed = a->pressed_id == DEVICE_BTN_REFRESH;
	ui_text(a->buf, "Refresh", BOT_W - PAD_X - 52.0f,
	        ui_baseline((HEADER_H - ui_px(TY_ROW_SUB)) / 2.0f, TY_ROW_SUB),
	        TY_ROW_SUB, 52.0f, refresh_pressed ? CLR_GREEN : CLR_SUB);
	tb_add(a->tb, BOT_W - 78.0f, 0.0f, 78.0f, HEADER_H, DEVICE_BTN_REFRESH);
}
