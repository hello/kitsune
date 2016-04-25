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


FRESULT hello_fs_mount (const char* drv,FATFS *fs) {

	FRESULT res;
	LOCK();
	res = f_mount(fs,drv,1);
	UNLOCK();
	return res;
}
FRESULT hello_fs_open ( FIL *fp, const char *path, BYTE mode) {
	FRESULT res;
	LOCK();
	res = f_open(fp,path,mode);
	f_sync(fp);
	UNLOCK();
	return res;
}

#include <stdbool.h>
bool add_to_file_error_queue(char* filename, int32_t err_code, bool write_error);
FRESULT hello_fs_read (FIL *fp, void *buff,UINT btr,UINT *br  ) {
	FRESULT res;
	LOCK();
	res = f_read(fp,buff,btr,br);
	UNLOCK();

	if(res)
	{
		add_to_file_error_queue(NULL, res, 0);
	}
	return res;
}
#include "uart_logger.h"
FRESULT hello_fs_write (  FIL *fp, const void *buff,UINT btw,UINT *bw) {
#define minval( a,b ) a < b ? a : b
	FRESULT res = FR_OK;
	LOCK();
	*bw = 0;
	if( btw > SD_BLOCK_SIZE ) {
		unsigned int wr = 0;
		int to_wr = btw; //signed!
		while( to_wr > 0 && res == FR_OK ) { //todo get multi block writes working
			res = f_write(fp,(uint8_t*)buff+wr,minval(to_wr, SD_BLOCK_SIZE),bw);
			to_wr-=*bw;
			wr+=*bw;
			*bw = 0;
		}
		*bw = wr;
	}
	else {
		res = f_write(fp,buff,btw,bw);
	}
	f_sync(fp);
	UNLOCK();

	if(res)
	{
		add_to_file_error_queue(NULL, res, 1);
	}

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
FRESULT hello_fs_closedir (DIR* dirobj) {
	FRESULT res;
	LOCK();
	res = f_closedir(dirobj);
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
#if 0
FRESULT hello_fs_mkfs (const char* drv, BYTE partition, UINT allocsize) {
	FRESULT res;
	LOCK();
	vTaskDelay(1000);
	res = f_mkfs(drv,partition,allocsize);
	UNLOCK();
	return res;
}
#endif
