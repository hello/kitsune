#include "FreeRTOS.h"
#include "semphr.h"
#include "hellofilesystem.h"

static xSemaphoreHandle _mutex = 0;

#define LOCK() \
	xSemaphoreTakeRecursive(_mutex,portMAX_DELAY);

#define UNLOCK()\
	xSemaphoreGiveRecursive(_mutex);

void hello_fs_init(void) {
	if (!_mutex) {
		_mutex = xSemaphoreCreateRecursiveMutex();
	}
}

void hello_fs_lock () {
	LOCK();
}
void hello_fs_unlock () {
	UNLOCK();
}


FRESULT hello_fs_mount (BYTE drv,FATFS *fs) {

	FRESULT res;
	LOCK();
	res = f_mount(drv,fs);
	UNLOCK();
	return res;
}
FRESULT hello_fs_open ( FIL *fp, const char *path, BYTE mode) {
	FRESULT res;
	LOCK();
	res = f_open(fp,path,mode);
	UNLOCK();
	return res;
}
FRESULT hello_fs_read (FIL *fp, void *buff,WORD btr,WORD *br  ) {
	FRESULT res;
	LOCK();
	res = f_read(fp,buff,btr,br);
	UNLOCK();
	return res;
}
FRESULT hello_fs_write (  FIL *fp, const void *buff,WORD btw,WORD *bw) {
	FRESULT res;
	LOCK();
	res = f_write(fp,buff,btw,bw);
	UNLOCK();
	return res;
}

FRESULT hello_fs_lseek(FIL *fp,DWORD ofs) {
	FRESULT res;
	LOCK();
	res = f_lseek(fp,ofs);
	UNLOCK();
	return res;
}
FRESULT hello_fs_close (FIL* fp) {
	FRESULT res;
	LOCK();
	res = f_close(fp);
	UNLOCK();
	return res;
}
FRESULT hello_fs_opendir (DIR* dirobj, const char* path) {
	FRESULT res;
	LOCK();
	res = f_opendir(dirobj,path);
	UNLOCK();
	return res;
}
FRESULT hello_fs_readdir (DIR * dirobj, FILINFO * finfo) {
	FRESULT res;
	LOCK();
	res = f_readdir(dirobj,finfo);
	UNLOCK();
	return res;
}
FRESULT hello_fs_stat (const char* path, FILINFO * finfo) {
	FRESULT res;
	LOCK();
	res = f_stat(path,finfo);
	UNLOCK();
	return res;
}
FRESULT hello_fs_getfree (const char * drv, DWORD * nclust, FATFS ** fatfs) {
	FRESULT res;
	LOCK();
	res = f_getfree(drv,nclust,fatfs);
	UNLOCK();
	return res;
}



FRESULT hello_fs_sync (FIL*finfo) {
	FRESULT res;
	LOCK();
	res = f_sync(finfo);
	UNLOCK();
	return res;
}
FRESULT hello_fs_unlink (const char*path) {
	FRESULT res;
	LOCK();
	res = f_unlink(path);
	UNLOCK();
	return res;
}
FRESULT    hello_fs_mkdir (const char*path) {
	FRESULT res;
	LOCK();
	res = f_mkdir(path);
	UNLOCK();
	return res;
}
FRESULT hello_fs_chmod (const char*path, BYTE value, BYTE mask) {
	FRESULT res;
	LOCK();
	res = f_chmod(path,value,mask);
	UNLOCK();
	return res;
}
FRESULT hello_fs_rename (const char* path_old, const char*path_new) {
	FRESULT res;
	LOCK();
	res = f_rename(path_old,path_new);
	UNLOCK();
	return res;
}
#include "FreeRTOS.h"
#include "task.h"
FRESULT hello_fs_mkfs (BYTE drv, BYTE partition, BYTE allocsize) {
	FRESULT res;
	LOCK();
	vTaskDelay(1000);
	res = f_mkfs(drv,partition,allocsize);
	UNLOCK();
	return res;
}
FRESULT hello_fs_append(const char* file_name, const unsigned char* content, int length) {
	FRESULT res;
	LOCK();
	res = f_append(file_name,content,length);
	UNLOCK();
	return res;
}

////==========================================================
//fs stream impl
#define FS_IO_CAP 512
#include <string.h>
typedef struct{
	FIL f;
	int32_t limit;
}fs_stream_t;

static int fs_write(void * ctx, const void * buf, size_t size){
	//due to a bug in the driver, only up to 512 works, so we cap it here
	fs_stream_t * fs = (fs_stream_t*)ctx;
	WORD size_to_write = (size > FS_IO_CAP)?FS_IO_CAP:size;
	FRESULT res = hello_fs_write(&fs->f,buf,size_to_write,&size_to_write);
	if(res != FR_OK){
		return HLO_STREAM_ERROR;
	}
	return (int)size_to_write;
}
static int fs_write_with_limit(void * ctx, const void * buf, size_t size){
	int ret = fs_write(ctx, buf, size);
	if(ret > 0){
		fs_stream_t * fs = (fs_stream_t*)ctx;
		fs->limit -= ret;
		if(fs->limit <= 0){
			return HLO_STREAM_EOF;
		}
	}
	return ret;
}

static int fs_read(void * ctx, void * buf, size_t size){
	fs_stream_t * fs = (fs_stream_t*)ctx;
	WORD read;
	int size_to_read = (size > FS_IO_CAP)?FS_IO_CAP:size;
	FRESULT res = hello_fs_read(&fs->f, buf, size_to_read, &read);
	if(read == 0){
		return HLO_STREAM_EOF;
	}
	if(res != FR_OK){
		return HLO_STREAM_ERROR;
	}
	return read;
}
static int fs_read_with_replay(void * ctx, void * buf, size_t size){
	int ret = fs_read(ctx, buf, size);
	if(ret == HLO_STREAM_EOF){
		fs_stream_t * fs = (fs_stream_t*)ctx;
		if(fs->limit != 0){
			if(fs->limit > 0){
				fs->limit--;
			}
			if(FR_OK == hello_fs_lseek(&fs->f,0)){
				return fs_read(ctx, buf, size);
			}else{//unable to rewind
				return HLO_STREAM_ERROR;
			}
		}
	}
	return ret;
}
static int fs_close(void * ctx){
	fs_stream_t * fs = (fs_stream_t*)ctx;
	hello_fs_close(&fs->f);
	vPortFree(fs);
	return 0;
}



hlo_stream_t * fs_stream_open(const char * path, uint32_t options){
	fs_stream_t * fs = pvPortMalloc(sizeof(fs_stream_t));
	hlo_stream_vftbl_t fs_stream_impl = {
			.write = fs_write,
			.read = fs_read,
			.close = fs_close
	};
	if(fs){
		FRESULT res;
		memset(fs, 0, sizeof(*fs));
		switch(options){
		case HLO_STREAM_WRITE:
			fs_stream_impl.write = NULL;
			if(FR_OK == (res = hello_fs_open(&fs->f, path, FA_WRITE|FA_OPEN_ALWAYS))){
				FILINFO info = {0};
				if(FR_OK == (res = hello_fs_stat(path,&info))){
					if(info.fsize > 0){
						hello_fs_lseek(&fs->f,info.fsize);
					}
				}
			}
			break;
		case HLO_STREAM_READ:
			fs_stream_impl.read = NULL;
			res = hello_fs_open(&fs->f, path, FA_READ);
			break;
		case HLO_STREAM_READ_WRITE:
			//not supported yet
		default:
			goto fail;
		}
		if(res != FR_OK){
fail:
			vPortFree(fs);
			return NULL;
		}
		return hlo_stream_new(&fs_stream_impl, fs, options);
	}else{
		return NULL;
	}

}
hlo_stream_t * fs_stream_open_media(const char * filepath, int32_t replay){
	hlo_stream_t * ret =  fs_stream_open(filepath, HLO_STREAM_READ);
	if(ret){
		fs_stream_t * fs = (fs_stream_t *)ret->ctx;
		fs->limit = replay;
		ret->impl.read = fs_read_with_replay;
	}
	return ret;
}
hlo_stream_t * fs_stream_open_wbuf(const char * filepath, int32_t limit){
	hlo_stream_t * ret =  fs_stream_open(filepath, HLO_STREAM_WRITE);
	if(ret){
		fs_stream_t * fs = (fs_stream_t *)ret->ctx;
		if(FR_OK != hello_fs_lseek(&fs->f,0)){
			hlo_stream_close(ret);
			return NULL;
		}
		fs->limit = limit;
		ret->impl.write = fs_write_with_limit;
	}
	return ret;
}
