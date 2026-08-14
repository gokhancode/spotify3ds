#pragma once

#include <citro2d.h>

#include "../spotify/player.h"
#include "../updater.h"
#include "touch.h"

enum {
	DEVICE_BTN_BACK = 1000,
	DEVICE_BTN_REFRESH,
	DEVICE_BTN_UPDATE,
	DEVICE_ROW0 = 1010, /* + device index */
};

typedef struct {
	C2D_TextBuf        buf;
	touch_builder     *tb;
	const device_list *devices;
	const char        *target_id;    /* selected target device id, or "" */
	const char        *active_id;    /* device currently playing, or "" */
	bool               loading;
	update_stage       update_stage; /* self-update progress for the footer */
	const char        *update_msg;   /* error detail when failed */
	int                pressed_id;
} screen_devices_args;

void screen_devices_draw(const screen_devices_args *a);
