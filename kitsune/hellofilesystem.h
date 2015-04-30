#ifndef _HELLOFILESYSTEM_H_
#define _HELLOFILESYSTEM_H_

#include "ff.h"
#include "hlo_stream.h"

void hello_fs_init(void);  /*  Initializes locking semaphore */
void hello_fs_lock(); //global SD card lock
void hello_fs_unlock();

FRESULT hello_fs_mount (BYTE, FATFS*);                        /* Mount/Unmount a logical drive */
FRESULT hello_fs_open (FIL*, const char*, BYTE);            /* Open or create a file */
FRESULT hello_fs_read (FIL*, void*, WORD, WORD*);            /* Read data from a file */
FRESULT hello_fs_write (FIL*, const void*, WORD, WORD*);    /* Write data to a file */
FRESULT hello_fs_lseek (FIL*, DWORD);                        /* Move file pointer of a file object */
FRESULT hello_fs_close (FIL*);                                /* Close an open file object */
FRESULT hello_fs_opendir (DIR*, const char*);                /* Open an existing directory */
FRESULT hello_fs_readdir (DIR*, FILINFO*);                    /* Read a directory item */
FRESULT hello_fs_stat (const char*, FILINFO*);                /* Get file status */
FRESULT hello_fs_getfree (const char*, DWORD*, FATFS**);    /* Get number of free clusters on the drive */
FRESULT hello_fs_sync (FIL*);                                /* Flush cached data of a writing file */
FRESULT hello_fs_unlink (const char*);                        /* Delete an existing file or directory */
FRESULT hello_fs_mkdir (const char*);                        /* Create a new directory */
FRESULT hello_fs_chmod (const char*, BYTE, BYTE);            /* Change file/dir attriburte */
FRESULT hello_fs_rename (const char*, const char*);        /* Rename/Move a file or directory */
FRESULT hello_fs_mkfs (BYTE, BYTE, BYTE);                    /* Create a file system on the drive */
FRESULT hello_fs_append(const char* file_name, const unsigned char* content, int length);

//read only implementation for now
hlo_stream_t * fs_stream_open(const char * filepath, uint32_t options);
#endif //_HELLOFILESYSTEM_H_

