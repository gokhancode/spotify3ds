#pragma once

#include <stdbool.h>

/* In-app self-update: download the latest release CIA from GitHub and install it
 * with the Application Manager, so updates no longer need a PC/QR + FBI.
 *
 * Requires: the two GitHub trust anchors pinned in tls.c, `am:net` in app.rsf,
 * and a CFW console (Luma signature patches) - the same conditions that let you
 * install the unsigned CIA with FBI in the first place. */

typedef enum {
	UPDATE_IDLE = 0,
	UPDATE_DOWNLOADING,
	UPDATE_INSTALLING,
	UPDATE_DONE,   /* installed; the user should exit and relaunch */
	UPDATE_FAILED, /* see the error message */
} update_stage;

/* Blocking: download + install. Call from the worker thread only. *stage is
 * advanced live so the UI can show progress. Returns true on success. */
bool updater_run(volatile update_stage *stage, char *err, int errlen);
