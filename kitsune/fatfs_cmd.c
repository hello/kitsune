//*****************************************************************************
//
// commands.c - FreeRTOS porting example on CCS4
//
// Copyright (c) 2012 Fuel7, Inc.
//
//*****************************************************************************

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include "fatfs_cmd.h"

/* Standard Stellaris includes */
#include "hw_types.h"

/* Other Stellaris include */
#include "uartstdio.h"

#include "fatfs_cmd.h"
#include "circ_buff.h"
#include "ff.h"
#include "diskio.h"


//*****************************************************************************
//
// Defines the size of the buffers that hold the path, or temporary data from
// the SD card.  There are two buffers allocated of this size.  The buffer size
// must be large enough to hold the longest expected full path name, including
// the file name, and a trailing null character.
//
//*****************************************************************************
#define PATH_BUF_SIZE           80

//*****************************************************************************
//
// This buffer holds the full path to the current working directory.  Initially
// it is root ("/").
//
//*****************************************************************************
static char cwd_buff[PATH_BUF_SIZE] = "/";

//*****************************************************************************
//
// A temporary data buffer used when manipulating file paths, or reading data
// from the SD card.
//
//*****************************************************************************
static char path_buff[PATH_BUF_SIZE];


//*****************************************************************************
//
// The following are data structures used by FatFs.
//
//*****************************************************************************
static FATFS fsobj;
static DIR fsdirobj;
static FILINFO file_info;
static FIL file_obj;

extern tCircularBuffer *pTxBuffer;

int Cmd_mnt(int argc, char *argv[])
{
    FRESULT res;

	res = f_mount(0, &fsobj);
	if(res != FR_OK)
	{
		UARTprintf("f_mount error: %i\n", (res));
		return(1);
	}
	return 0;
}
int Cmd_umnt(int argc, char *argv[])
{
    FRESULT res;

	res = f_mount(0, NULL);
	if(res != FR_OK)
	{
		UARTprintf("f_mount error: %i\n", (res));
		return(1);
	}
	UARTprintf("f_mount success\n");
	return 0;
}

int Cmd_mkfs(int argc, char *argv[])
{
    FRESULT res;

	UARTprintf("\n\nMaking FS...\n");

	res = f_mkfs(0, 1, 16);
	if(res != FR_OK)
	{
		UARTprintf("f_mkfs error: %i\n", (res));
		return(1);
	}
	UARTprintf("f_mkfs success\n");
	return 0;
}

//*****************************************************************************
//
// This function implements the "ls" command.  It opens the current directory
// and enumerates through the contents, and prints a line for each item it
// finds.  It shows details such as file attributes, time and date, and the
// file size, along with the name.  It shows a summary of file sizes at the end
// along with free space.
//
//*****************************************************************************
int
Cmd_ls(int argc, char *argv[])
{
    uint32_t ui32TotalSize;
    uint32_t ui32FileCount;
    uint32_t ui32DirCount;
    FRESULT res;
    FATFS *psFatFs;

    res = f_opendir(&fsdirobj,cwd_buff);

    if(res != FR_OK)
    {
        return((int)res);
    }

    ui32TotalSize = 0;
    ui32FileCount = 0;
    ui32DirCount = 0;

    UARTprintf("\n");

    for(;;)
    {
        res = f_readdir(&fsdirobj, &file_info);

        if(res != FR_OK)
        {
            return((int)res);
        }

        // If the file name is blank, then this is the end of the listing.
        if(!file_info.fname[0])
        {
            break;
        }

        // If the attribue is directory, then increment the directory count.
        if(file_info.fattrib & AM_DIR)
        {
            ui32DirCount++;
        }

        // Otherwise, it is a file.  Increment the file count, and add in the
        // file size to the total.
        else
        {
            ui32FileCount++;
            ui32TotalSize += file_info.fsize;
        }

        // Print the entry information on a single line with formatting to show
        // the attributes, date, time, size, and name.
        UARTprintf("%c%c%c%c%c %u/%02u/%02u %02u:%02u %9u  %s\n",
                   (file_info.fattrib & AM_DIR) ? 'D' : '-',
                   (file_info.fattrib & AM_RDO) ? 'R' : '-',
                   (file_info.fattrib & AM_HID) ? 'H' : '-',
                   (file_info.fattrib & AM_SYS) ? 'S' : '-',
                   (file_info.fattrib & AM_ARC) ? 'A' : '-',
                   (file_info.fdate >> 9) + 1980,
                   (file_info.fdate >> 5) & 15,
                   file_info.fdate & 31,
                   (file_info.ftime >> 11),
                   (file_info.ftime >> 5) & 63,
                   file_info.fsize,
                   file_info.fname);
    }

    // Print summary lines showing the file, dir, and size totals.
    UARTprintf("\n%4u File(s),%10u bytes total\n%4u Dir(s)",
                ui32FileCount, ui32TotalSize, ui32DirCount);

    // Get the free space.
    res = f_getfree("/", (DWORD *)&ui32TotalSize, &psFatFs);

    // Check for error and return if there is a problem.
    if(res != FR_OK)
    {
        return((int)res);
    }

    // Display the amount of free space that was calculated.
    UARTprintf(", %10uK bytes free\n", (ui32TotalSize *
                                        psFatFs->sects_clust / 2));

    return(0);
}

//*****************************************************************************
//
// This function implements the "cd" command.  It takes an argument that
// specifies the directory to make the current working directory.  Path
// separators must use a forward slash "/".  The argument to cd can be one of
// the following:
//
// * root ("/")
// * a fully specified path ("/my/path/to/mydir")
// * a single directory name that is in the current directory ("mydir")
// * parent directory ("..")
//
// It does not understand relative paths, so dont try something like this:
// ("../my/new/path")
//
// Once the new directory is specified, it attempts to open the directory to
// make sure it exists.  If the new path is opened successfully, then the
// current working directory (cwd) is changed to the new path.
//
//*****************************************************************************
int
Cmd_cd(int argc, char *argv[])
{
    uint_fast8_t ui8Idx;
    FRESULT res;

    // Copy the current working path into a temporary buffer so it can be
    // manipulated.
    strcpy(path_buff, cwd_buff);

    // If the first character is /, then this is a fully specified path, and it
    // should just be used as-is.
    if(argv[1][0] == '/')
    {
        // Make sure the new path is not bigger than the cwd buffer.
        if(strlen(argv[1]) + 1 > sizeof(cwd_buff))
        {
            UARTprintf("Resulting path name is too long\n");
            return(0);
        }

        // If the new path name (in argv[1])  is not too long, then copy it
        // into the temporary buffer so it can be checked.
        else
        {
            strncpy(path_buff, argv[1], sizeof(path_buff));
        }
    }
    // If the argument is .. then attempt to remove the lowest level on the
    // CWD.
    else if(!strcmp(argv[1], ".."))
    {
        // Get the index to the last character in the current path.
        ui8Idx = strlen(path_buff) - 1;

        // Back up from the end of the path name until a separator (/) is
        // found, or until we bump up to the start of the path.
        while((path_buff[ui8Idx] != '/') && (ui8Idx > 1))
        {
            // Back up one character.
            ui8Idx--;
        }

        // Now we are either at the lowest level separator in the current path,
        // or at the beginning of the string (root).  So set the new end of
        // string here, effectively removing that last part of the path.
        path_buff[ui8Idx] = 0;
    }

    // Otherwise this is just a normal path name from the current directory,
    // and it needs to be appended to the current path.
    else
    {
        // Test to make sure that when the new additional path is added on to
        // the current path, there is room in the buffer for the full new path.
        // It needs to include a new separator, and a trailing null character.
        if(strlen(path_buff) + strlen(argv[1]) + 1 + 1 > sizeof(cwd_buff))
        {
            UARTprintf("Resulting path name is too long\n");
            return(0);
        }

        // The new path is okay, so add the separator and then append the new
        // directory to the path.
        else
        {
            // If not already at the root level, then append a /
            if(strcmp(path_buff, "/"))
            {
                strcat(path_buff, "/");
            }

            // Append the new directory to the path.
            strcat(path_buff, argv[1]);
        }
    }

    // At this point, a candidate new directory path is in chTmpBuf.  Try to
    // open it to make sure it is valid.
    res = f_opendir(&fsdirobj,path_buff);

    // If it can't be opened, then it is a bad path.  Inform the user and
    // return.
    if(res != FR_OK)
    {
        UARTprintf("cd: %s\n",path_buff);
        return((int)res);
    }

    // Otherwise, it is a valid new path, so copy it into the CWD.
    else
    {
        strncpy(cwd_buff,path_buff, sizeof(cwd_buff));
    }
    return(0);
}

//*****************************************************************************
//
// This function implements the "pwd" command.  It simply prints the current
// working directory.
//
//*****************************************************************************
int
Cmd_pwd(int argc, char *argv[])
{
    UARTprintf("%s\n", cwd_buff);
    return(0);
}
int global_filename(char * local_fn)
{
    // First, check to make sure that the current path (CWD), plus the file
    // name, plus a separator and trailing null, will all fit in the temporary
    // buffer that will be used to hold the file name.  The file name must be
    // fully specified, with path, to FatFs.
    if(strlen(cwd_buff) + strlen(local_fn) + 1 + 1 > sizeof(path_buff))
    {
        UARTprintf("Resulting path name is too long\n");
        return(0);
    }

    // Copy the current path to the temporary buffer so it can be manipulated.
    strcpy(path_buff, cwd_buff);

    // If not already at the root level, then append a separator.
    if(strcmp("/", cwd_buff))
    {
        strcat(path_buff, "/");
    }

    // Now finally, append the file name to result in a fully specified file.
    strcat(path_buff, local_fn);

    return 0;
}
//*****************************************************************************
//
// This function implements the "cat" command.  It reads the contents of a file
// and prints it to the console.  This should only be used on text files.  If
// it is used on a binary file, then a bunch of garbage is likely to printed on
// the console.
//
//*****************************************************************************
int
Cmd_cat(int argc, char *argv[])
{
    FRESULT res;
    uint16_t ui16BytesRead;

    if(global_filename( argv[1] ))
    {
    	return 1;
    }

    res = f_open(&file_obj, path_buff, FA_READ);
    if(res != FR_OK)
    {
        return((int)res);
    }

    // Enter a loop to repeatedly read data from the file and display it, until
    // the end of the file is reached.
    do
    {
        // Read a block of data from the file.  Read as much as can fit in the
        // temporary buffer, including a space for the trailing null.
        res = f_read(&file_obj, path_buff, 4,
                         &ui16BytesRead);

        // If there was an error reading, then print a newline and return the
        // error to the user.
        if(res != FR_OK)
        {
            UARTprintf("\n");
            return((int)res);
        }
        // Null terminate the last block that was read to make it a null
        // terminated string that can be used with printf.
        path_buff[ui16BytesRead] = 0;

        // Print the last chunk of the file that was received.
        UARTprintf("%s", path_buff);

    }
    while(ui16BytesRead == 4);

    f_close( &file_obj );

    UARTprintf("\n");
    return(0);
}

int
Cmd_write_audio(char *argv[])
{
    FRESULT res;

	WORD bytes = 0;
	WORD bytes_written = 0;
	WORD bytes_to_write = strlen(argv[1])+1;

    if(global_filename( "TXBUF"))
    {
    	return 1;
    }
    UARTprintf("print");
    // Open the file for reading.
    //res = f_open(&file_obj, path_buff, FA_CREATE_NEW|FA_WRITE);
    res = f_open(&file_obj, path_buff, FA_WRITE);
    f_stat( path_buff, &file_info );

    if( file_info.fsize != 0 )
        res = f_lseek(&file_obj, file_info.fsize );

    do {
		res = f_write( &file_obj, argv[1]+bytes_written, bytes_to_write-bytes_written, &bytes );
		bytes_written+=bytes;
    } while( bytes_written < bytes_to_write );

    res = f_close( &file_obj );

    if(res != FR_OK)
    {
        return((int)res);
    }
    return(0);
}

int
Cmd_write(int argc, char *argv[])
{
	UARTprintf("Cmd_write\n");

	WORD bytes = 0;
	WORD bytes_written = 0;
	WORD bytes_to_write = strlen(argv[2]) + 1;

    if(global_filename( argv[1] ))
    {
    	return 1;

    }

    // Open the file for writing.
    FRESULT res = f_open(&file_obj, path_buff, FA_CREATE_NEW|FA_WRITE|FA_OPEN_ALWAYS);
    UARTprintf("res :%d\n",res);

    if(res != FR_OK && res != FR_EXIST){
    	UARTprintf("File open %s failed: %d\n", path_buff, res);
    	return res;
    }

    f_stat( path_buff, &file_info );

    if( file_info.fsize != 0 )
        res = f_lseek(&file_obj, file_info.fsize );

    do {
		res = f_write( &file_obj, argv[2]+bytes_written, bytes_to_write-bytes_written, &bytes );
		bytes_written+=bytes;
		UARTprintf("bytes written: %d\n", bytes_written);

    } while( bytes_written < bytes_to_write );

    res = f_close( &file_obj );

    if(res != FR_OK)
    {
        return((int)res);
    }
    return(0);
}

int
Cmd_append(int argc, char *argv[])
{
	return f_append((const char *)argv[1], (const unsigned char *)argv[2], strlen(argv[2])) != FR_OK;
}



// add this for creating buff for sound recording
int Cmd_write_record(int argc, char *argv[])
//int Cmd_write_record(int argc, char *argv[])
{
	//#define RECORD_SIZE 4
	//unsigned char argv[1][RECORD_SIZE];

				//argv[1][0] = 0xAA;
//				argv[1][1] = 0x78;
//				argv[1][2] = 0x55;
//				argv[1][3] = 0x50;

    FRESULT res;

	WORD bytes = 0;
	WORD bytes_written = 0;
	WORD bytes_to_write = strlen(argv[1]) * sizeof(char)+1;
//	WORD bytes_to_write = strlen(argv[1][1]) * 4 +1;
    if(global_filename( "VONE" ))
    {
    	return 1;
    }

    // Open the file for reading.
    //res = f_open(&file_obj, path_buff, FA_CREATE_NEW|FA_WRITE);
    res = f_open(&file_obj, path_buff, FA_WRITE|FA_OPEN_EXISTING|FA_CREATE_NEW);

    f_stat( path_buff, &file_info );

    if( file_info.fsize != 0 )
        res = f_lseek(&file_obj, file_info.fsize );

    do {
		res = f_write( &file_obj, argv[1]+bytes_written, bytes_to_write-bytes_written, &bytes );
		bytes_written+=bytes;
    } while( bytes_written < bytes_to_write );

    res = f_close( &file_obj );

    if(res != FR_OK)
    {
        return((int)res);
    }
    //UARTprintf("%s", path_buff);
    return(0);
}
// end sound recording buffer
int
Cmd_rm(int argc, char *argv[])
{
    FRESULT res;

    if(global_filename( argv[1] ))
    {
    	return 1;
    }

    res = f_unlink(path_buff);

    if(res != FR_OK)
    {
        return((int)res);
    }

    return(0);
}

int
Cmd_mkdir(int argc, char *argv[])
{
    FRESULT res;

    if(global_filename( argv[1] ))
    {
    	return 1;
    }

    res = f_mkdir(path_buff);
    if(res != FR_OK)
    {
        return((int)res);
    }
    return(0);
}

