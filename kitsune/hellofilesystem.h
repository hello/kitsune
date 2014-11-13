#ifndef _HELLOFILESYSTEM_H_
#define _HELLOFILESYSTEM_H_

#include "ff.h"


FRESULT hello_mount (BYTE, FATFS*);                        /* Mount/Unmount a logical drive */
FRESULT hello_open (FIL*, const char*, BYTE);            /* Open or create a file */
FRESULT hello_read (FIL*, void*, WORD, WORD*);            /* Read data from a file */
FRESULT hello_write (FIL*, const void*, WORD, WORD*);    /* Write data to a file */
FRESULT hello_lseek (FIL*, DWORD);                        /* Move file pointer of a file object */
FRESULT hello_close (FIL*);                                /* Close an open file object */
FRESULT hello_opendir (DIR*, const char*);                /* Open an existing directory */
FRESULT hello_readdir (DIR*, FILINFO*);                    /* Read a directory item */
FRESULT hello_stat (const char*, FILINFO*);                /* Get file status */
FRESULT hello_getfree (const char*, DWORD*, FATFS**);    /* Get number of free clusters on the drive */
FRESULT hello_sync (FIL*);                                /* Flush cached data of a writing file */
FRESULT hello_unlink (const char*);                        /* Delete an existing file or directory */
FRESULT hello_mkdir (const char*);                        /* Create a new directory */
FRESULT hello_chmod (const char*, BYTE, BYTE);            /* Change file/dir attriburte */
FRESULT hello_rename (const char*, const char*);        /* Rename/Move a file or directory */
FRESULT hello_mkfs (BYTE, BYTE, BYTE);                    /* Create a file system on the drive */
FRESULT hello_append(const char* file_name, const unsigned char* content, int length);


#endif //_HELLOFILESYSTEM_H_

