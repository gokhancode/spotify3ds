#include "updater.h"

#include <3ds.h>
#include <stdio.h>
#include <string.h>

#include "net/http.h"
#include "testlog.h"

/* Always the newest release asset; the same URL the install QR encodes. */
#define UPDATE_URL \
	"https://github.com/gokhancode/spotify3ds/releases/latest/download/Spotify3DS.cia"

#define MIN_CIA_BYTES 100000u
#define WRITE_CHUNK   0x40000u /* 256 KiB */

bool updater_run(volatile update_stage *stage, char *err, int errlen)
{
	*stage = UPDATE_DOWNLOADING;

	http_response r;
	if (!http_get_follow(UPDATE_URL, &r, 6, err, errlen)) {
		*stage = UPDATE_FAILED;
		return false;
	}
	if (r.status != 200 || !r.body || r.body_len < MIN_CIA_BYTES) {
		snprintf(err, errlen, "download failed (http %d, %u bytes)", r.status,
		         (unsigned)r.body_len);
		http_free(&r);
		*stage = UPDATE_FAILED;
		return false;
	}

	/* A CIA begins with its 32-bit header size, 0x00002020. Guards against a
	 * stray HTML error page being fed to the installer. */
	const unsigned char *b = (const unsigned char *)r.body;
	if (!(b[0] == 0x20 && b[1] == 0x20 && b[2] == 0x00 && b[3] == 0x00)) {
		snprintf(err, errlen, "not a CIA (bad header)");
		http_free(&r);
		*stage = UPDATE_FAILED;
		return false;
	}

	tl_log("update: downloaded %u bytes, installing", (unsigned)r.body_len);
	*stage = UPDATE_INSTALLING;

	Result res = amInit();
	if (R_FAILED(res)) {
		snprintf(err, errlen, "amInit 0x%08lX", (unsigned long)res);
		http_free(&r);
		*stage = UPDATE_FAILED;
		return false;
	}

	Handle cia = 0;
	res = AM_StartCiaInstall(MEDIATYPE_SD, &cia);
	if (R_FAILED(res)) {
		snprintf(err, errlen, "StartCiaInstall 0x%08lX", (unsigned long)res);
		amExit();
		http_free(&r);
		*stage = UPDATE_FAILED;
		return false;
	}

	const u8 *data = (const u8 *)r.body;
	u64       off = 0;
	size_t    remain = r.body_len;
	bool      ok = true;
	while (remain > 0) {
		const u32 chunk = remain > WRITE_CHUNK ? WRITE_CHUNK : (u32)remain;
		u32       written = 0;
		res = FSFILE_Write(cia, &written, off, data, chunk, 0);
		if (R_FAILED(res) || written == 0) {
			ok = false;
			break;
		}
		off += written;
		data += written;
		remain -= written;
	}

	if (!ok) {
		snprintf(err, errlen, "write failed 0x%08lX", (unsigned long)res);
		AM_CancelCIAInstall(cia);
		amExit();
		http_free(&r);
		*stage = UPDATE_FAILED;
		return false;
	}

	res = AM_FinishCiaInstall(cia);
	amExit();
	http_free(&r);

	if (R_FAILED(res)) {
		snprintf(err, errlen, "install 0x%08lX", (unsigned long)res);
		*stage = UPDATE_FAILED;
		return false;
	}

	tl_log("update: installed ok");
	*stage = UPDATE_DONE;
	return true;
}
