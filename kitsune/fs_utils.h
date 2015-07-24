#ifndef __FS_UTILS_H__
#define __FS_UTILS_H__
#include "hellofilesystem.h"

typedef void (*file_itr) (const char * file_name, const FILINFO * info);

int fs_get( char * file, void * data, int max_rd, int * len );
int fs_save( char* file, void* data, int len);
int fs_list( char * dir, file_itr itr);
#endif
