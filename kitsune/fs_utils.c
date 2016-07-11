#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kit_assert.h"
#include <stdint.h>

#include "simplelink.h"
#include "sl_sync_include_after_simplelink_header.h"

#include "uart_logger.h"

#include "fs_utils.h"

int fs_get( char * file, void * data, int max_rd, int * len ) {
	long hndl = -1;
	int RetVal, Offset;

	// read in aes key
	hndl = sl_FsOpen((const _u8*)file, SL_FS_READ, NULL );
	if (hndl < 0) {
		LOGE("failed to open %s\n", file);
		return hndl;
	}

	Offset = 0;
	RetVal = sl_FsRead(hndl, Offset, data, max_rd);
	if ( 0 > RetVal ) {
		LOGE("failed to read %s\n", file);
		sl_FsClose(hndl, NULL, NULL, 0);
		return RetVal;
	}
	if( len ) {
		*len = RetVal;
	}
	return sl_FsClose(hndl, NULL, NULL, 0);
}
int fs_save( char* file, void* data, int len) {
	unsigned long tok=0;
	long hndl, bytes;
	SlFsFileInfo_t info;

	sl_FsGetInfo((unsigned char*)file, tok, &info);

	if ( hndl = sl_FsOpen((unsigned char*)file, SL_FS_WRITE, &tok) < 0) {
		LOGI("error opening file, trying to create\n");

		if (hndl = sl_FsOpen((unsigned char*)file,
				SL_FS_CREATE_NOSIGNATURE | SL_FS_CREATE_MAX_SIZE( 65535 ), &tok) < 0) {
			LOGI("error opening for write\n");
			return -1;
		}else{
			sl_FsWrite(hndl, 0, data, 1);  // Dummy write, we don't care about the result
		}
	}

	bytes = sl_FsWrite(hndl, 0, (_u8*)data, len);
	if( bytes != len ) {
		LOGE( "writing %s failed %d", file, bytes );
		sl_FsClose(hndl, 0, 0, 0);
		return -2;
	}
	sl_FsClose(hndl, 0, 0, 0);
	return 0;
}
