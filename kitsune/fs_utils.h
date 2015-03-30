#ifndef __FS_UTILS_H__
#define __FS_UTILS_H__

int fs_get( char * file, void * data, int max_rd, int * len );
int fs_save( char* file, void* data, int len);
#endif
