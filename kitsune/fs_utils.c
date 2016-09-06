#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "kit_assert.h"
#include <stdint.h>

#include "simplelink.h"
#include "sl_sync_include_after_simplelink_header.h"

#include "uart_logger.h"

#include "fs_utils.h"
#include "fs.h"


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
	hndl = sl_FsOpen((unsigned char*)file, SL_FS_WRITE, &tok);
	if (hndl < 0) {
		LOGI("error opening file, trying to create %d\n", hndl);
		hndl = sl_FsOpen((unsigned char*)file,
				SL_FS_CREATE|SL_FS_OVERWRITE | SL_FS_CREATE_NOSIGNATURE | SL_FS_CREATE_MAX_SIZE( 65535 ), &tok);
		if (hndl < 0) {
			LOGI("error opening for write %d\n", hndl);
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
//================================
//serial flash
typedef struct{
	long hndl;
	long tok;
	uint32_t size;
	uint32_t offset;
	uint32_t percent;
}sf_ctx_t;
static int _read_sf(void * ctx, void * out, size_t len){
	sf_ctx_t * sf = (sf_ctx_t*)ctx;
	int ret =  sl_FsRead(sf->hndl, sf->offset, out, len);
	if( ret > 0 ){
		sf->offset += ret;
		return ret;
	}else if(ret == 0 || ret == SL_ERROR_FS_OFFSET_OUT_OF_RANGE){
		return HLO_STREAM_EOF;
	}else{
		LOGW("SF error %d\r\n", ret);
		return HLO_STREAM_ERROR;
	}
}
static int _write_sf(void * ctx, const void * in, size_t len){
	sf_ctx_t * sf = (sf_ctx_t*)ctx;
	int ret = sl_FsWrite(sf->hndl, sf->offset, (uint8_t*)in, len);
	if( ret > 0 ){
		sf->offset += ret;

		if( sf->percent != 100*sf->offset / sf->size) {
			LOGI("DL %d\r\n", 100*sf->offset / sf->size );
			sf->percent = 100*sf->offset / sf->size;
		}
		return ret;
	}else if(ret == 0){
		LOGI("100\r\n" );
		return HLO_STREAM_EOF;
	}else{
		LOGW("SF error %d\r\n", ret);
        /* Close file without saving */
		sl_FsClose(sf->hndl, 0, (unsigned char*) "A", 1);
		return HLO_STREAM_ERROR;
	}
}
static int _close_sf(void * ctx){
	sf_ctx_t * sf = (sf_ctx_t*)ctx;
	sl_FsClose(sf->hndl, 0, 0, 0);
	return 0;
}
hlo_stream_t * open_serial_flash( char * filepath, uint32_t options, uint32_t max_size){
	long hndl, tok;
	int ret;
	SlFsFileInfo_t info;
	hlo_stream_vftbl_t tbl = (hlo_stream_vftbl_t){
		.close = _close_sf,
		.read = NULL,
		.write = NULL,
	};

	ret = sl_FsGetInfo((const uint8_t*)filepath, 0, &info);

	if(options == HLO_STREAM_READ){
		tbl.read = _read_sf;
		if ( ret == SL_ERROR_FS_FILE_NOT_EXISTS ){
			LOGE("Serial flash file %s does not exist.\r\n", filepath);
			return NULL;
		}else if((hndl =  sl_FsOpen((const uint8_t*)filepath, SL_FS_READ, NULL)) < 0 ){
			LOGE("Unable to open file for read\r\n");
			return NULL;
		}
	}else if(options == HLO_STREAM_WRITE){
		tbl.write = _write_sf;
		if ( ret == 0 ){ //if file exists
			if(sl_FsDel((const uint8_t*)filepath, 0) != 0){
				LOGE("Unable to open file for write\r\n");
				return NULL;
			}
		}
		uint8_t data[1] = {0};
		if((hndl = sl_FsOpen((const uint8_t*)filepath,
				  SL_FS_CREATE|SL_FS_OVERWRITE | SL_FS_CREATE_NOSIGNATURE | SL_FS_CREATE_MAX_SIZE( max_size ),
				(_u32*)&tok)) < 0){
			return NULL;
		}
		//sl_FsWrite(hndl, 0, data, 1);
	}else{
		LOGE("serial flash may not operate in duplex mode\r\n");
		return NULL;
	}
	sf_ctx_t * ctx = pvPortMalloc(sizeof(*ctx));
	assert(ctx);
	ctx->hndl = hndl;
	ctx->tok = tok;
	ctx->size = max_size;
	ctx->offset = 0;
	ctx->percent = 0;
	return hlo_stream_new(&tbl,ctx,options);
}
