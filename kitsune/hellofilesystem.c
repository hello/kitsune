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
typedef struct{
	FIL f;
	char fname[8+1+3+1];
}fs_stream_t;

static int fs_write(void * ctx, const void * buf, size_t size){
	fs_stream_t * fs = (fs_stream_t*)ctx;
	FRESULT res = hello_fs_append(fs->fname, buf, size);
	if(res != FR_OK){
		return ERROR;
	}
	return size;
}

static int fs_read(void * ctx, void * buf, size_t size){
	fs_stream_t * fs = (fs_stream_t*)ctx;
	WORD read;
	FRESULT res = hello_fs_read(&fs->f, buf, size, &read);
	if(res != FR_OK || read == 0){
		return ERROR;
	}
	return read;
}
static int fs_close(void * ctx){
	fs_stream_t * fs = (fs_stream_t*)ctx;
	hello_fs_close(&fs->f);
	vPortFree(fs);
	return 0;
}

static hlo_stream_vftbl_t fs_stream_impl = {
		.write = fs_write,
		.read = fs_read,
		.close = fs_close
};

hlo_stream_t * fs_stream_open(const char * path, uint32_t options){
	fs_stream_t * fs = pvPortMalloc(sizeof(fs_stream_t));
	if(fs){
		FRESULT res;
		memset(fs, 0, sizeof(*fs));
		//file name to long maybe
		if(strlen(path) >= sizeof(fs->fname)){
			goto fail;
		}
		strcpy(fs->fname,path);
		switch(options){
		case HLO_STREAM_IN:
			res = hello_fs_open(&fs->f, path, FA_CREATE_NEW|FA_WRITE|FA_OPEN_ALWAYS);
			break;
		case HLO_STREAM_OUT:
			res = hello_fs_open(&fs->f, path, FA_READ);
			break;
		case HLO_STREAM_IN_OUT:
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
