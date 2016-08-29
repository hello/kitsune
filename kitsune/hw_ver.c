#include "hw_ver.h"

#include "uart_logger.h"

#include "fs.h"
#include "fs_utils.h"
#include "sl_sync_include_after_simplelink_header.h"

#define DVT_STRING "dvt"
#define PVT_STRING "pvt"
#define EVT1_1p5_STRING "1.5 evt1"
int hw_ver = EVT1_1p5;

#define minval(a,b) a < b ? a : b;

static int check_ver_string(char *str, char *buf, int len) {
	return len >= strlen(str) && 0 == strncmp(buf, str, strlen(str));
}

void check_hw_version() {
#define BUF_SZ 64
#define HW_VER_FILE "/hello/hw_ver"

	unsigned long tok = 0;
	long hndl, err, bytes;
	SlFsFileInfo_t info;
	char buffer[BUF_SZ];

	sl_FsGetInfo(HW_VER_FILE, tok, &info);

	hndl = sl_FsOpen(HW_VER_FILE, SL_FS_READ, &tok);
	if (hndl < 0) {
		LOGI("error opening for read %d\n", hndl);
		return;
	}
	int min_len = minval(info.Len, BUF_SZ);
	bytes = sl_FsRead(hndl, 0, (unsigned char* ) buffer, min_len);

	if ( check_ver_string(DVT_STRING, buffer, bytes) ) {
		hw_ver = DVT;
	} else if ( check_ver_string(PVT_STRING, buffer, bytes) ) {
		hw_ver = PVT;
	} else if ( check_ver_string(EVT1_1p5_STRING, buffer, bytes) ) {
		hw_ver = EVT1_1p5;
	}

	sl_FsClose(hndl, 0, 0, 0);
}
int get_hw_ver() {
	return hw_ver;
}

int Cmd_hwver(int argc, char *argv[]) {
	DISP("Setting version %s\r\n", argv[1] );
	fs_save(HW_VER_FILE, argv[1], 2);
	return 0;
}
