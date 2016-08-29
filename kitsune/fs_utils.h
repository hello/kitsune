#ifndef __FS_UTILS_H__
#define __FS_UTILS_H__

int fs_get( char * file, void * data, int max_rd, int * len );
int fs_save( char* file, void* data, int len);

#include "hlo_stream.h"
hlo_stream_t * open_serial_flash( char * filepath, uint32_t options, uint32_t max_size);
#endif
