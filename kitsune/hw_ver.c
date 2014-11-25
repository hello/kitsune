#include "hw_ver.h"

#include "uart_logger.h"

#include "fs.h"
#include "sl_sync_include_after_simplelink_header.h"

#define EVT_STRING "evt2"
#define DVT_STRING "dvt"
#define PVT_STRING "pvt"
int hw_ver = EVT2;

#define minval(a,b) a < b ? a : b;

static int check_ver_string(char *str, char *buf, int len) {
	return len > strlen(str) && 0 == strncmp(buf, str, strlen(str));
}

void check_hw_version() {
#define BUF_SZ 64
#define HW_VER_FILE "/hello/hw_ver"

	unsigned long tok = 0;
	long hndl, err, bytes;
	SlFsFileInfo_t info;
	char buffer[BUF_SZ];

	sl_FsGetInfo(HW_VER_FILE, tok, &info);

	err = sl_FsOpen(HW_VER_FILE, FS_MODE_OPEN_READ, &tok, &hndl);
	if (err) {
		LOGI("error opening for read %d\n", err);
		return;
	}
	int min_len = minval(info.FileLen, BUF_SZ);
	bytes = sl_FsRead(hndl, 0, (unsigned char* ) buffer, min_len);

	if ( check_ver_string(EVT_STRING, buffer, bytes) ) {
		hw_ver = EVT2;
	} else if ( check_ver_string(DVT_STRING, buffer, bytes) ) {
		hw_ver = DVT;
	} else if ( check_ver_string(PVT_STRING, buffer, bytes) ) {
		hw_ver = PVT;
	}

	sl_FsClose(hndl, 0, 0, 0);
}
int get_hw_ver() {
	return hw_ver;
}
