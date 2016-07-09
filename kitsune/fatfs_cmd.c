//*****************************************************************************
//
// commands.c - FreeRTOS porting example on CCS4
//
// Copyright (c) 2012 Fuel7, Inc.
//
//*****************************************************************************

#include <stdio.h>
#include <string.h>
#include "netcfg.h"
#include "kit_assert.h"
#include <stdint.h>
#include "fatfs_cmd.h"

/* Standard Stellaris includes */
#include "hw_types.h"

/* Other Stellaris include */
#include "uartstdio.h"

#include "fatfs_cmd.h"
#include "circ_buff.h"
#include "hellofilesystem.h"
#include "diskio.h"

#include "FreeRTOS.h"

#include "kitsune_version.h"

//begin download stuff
#include "wlan.h"
#include "simplelink.h"
#include "socket.h"
#include "protocol.h"
#include "common.h"
#include "sl_sync_include_after_simplelink_header.h"

/* Hardware library includes. */
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_types.h"
#include "hw_ints.h"
#include "hw_wdt.h"
#include "wdt.h"
#include "wdt_if.h"
#include "rom.h"
#include "rom_map.h"

#define HLO_HTTP_ERR -1000
#define HTTP_RETRIES 5

#define PREFIX_BUFFER    "GET "
//#define POST_BUFFER      " HTTP/1.1\nAccept: text/html, application/xhtml+xml, */*\n\n"
#define POST_BUFFER_1  " HTTP/1.1\r\nHost:"
#define POST_BUFFER_2  "\r\nAccept: text/html, application/xhtml+xml, */*\r\n\r\n"

#define HTTP_FORBIDDEN         "403 Forbidden" //url expired
#define HTTP_FILE_NOT_FOUND    "404 Not Found" /* HTTP file not found response */
#define HTTP_STATUS_OK         "200 OK"  /* HTTP status ok response */
#define HTTP_CONTENT_LENGTH    "Content-Length:"  /* HTTP content length header */
#define HTTP_TRANSFER_ENCODING "Transfer-Encoding:" /* HTTP transfer encoding header */
#define HTTP_ENCODING_CHUNKED  "chunked" /* HTTP transfer encoding header value */
#define HTTP_CONNECTION        "Connection:" /* HTTP Connection header */
#define HTTP_CONNECTION_CLOSE  "close"  /* HTTP Connection header value */

#define DNS_RETRY           5 /* No of DNS tries */
#define SOCK_RETRY      256
#define HTTP_END_OF_HEADER  "\r\n\r\n"  /* string marking the end of headers in response */

#define MAX_BUFF_SIZE      (5*SD_BLOCK_SIZE)

int sf_sha1_verify(const char * sha_truth, const char * serial_file_path);
long bytesReceived = 0; // variable to store the file size
static int dl_sock = -1;
//end download stuff

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


int Cmd_mnt(int argc, char *argv[])
{
    FRESULT res;

	res = hello_fs_mount("0", &fsobj);
	if(res != FR_OK)
	{
		LOGF("f_mount error: %i\n", (res));
		return(1);
	}
	return 0;
}
int Cmd_umnt(int argc, char *argv[])
{
    FRESULT res;

	res = hello_fs_mount("0", NULL);
	if(res != FR_OK)
	{
		LOGF("f_mount error: %i\n", (res));
		return(1);
	}
	LOGF("f_mount success\n");
	return 0;
}
#if 0
int Cmd_mkfs(int argc, char *argv[])
{
    FRESULT res;

    LOGF("\n\nMaking FS...\n");

	res = hello_fs_mkfs("0", 0, 64);
	if(res != FR_OK)
	{
		LOGF("f_mkfs error: %i\n", (res));
		return(1);
	}
	LOGF("f_mkfs success\n");
	return 0;
}
#endif
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

    res = hello_fs_opendir(&fsdirobj,cwd_buff);

    if(res != FR_OK)
    {
        return((int)res);
    }

    ui32TotalSize = 0;
    ui32FileCount = 0;
    ui32DirCount = 0;

    LOGF("\n");

    for(;;)
    {
        res = hello_fs_readdir(&fsdirobj, &file_info);

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
        vTaskDelay(10);
        LOGF("%c%c%c%c%c %u/%02u/%02u %02u:%02u %9u  %s\n",
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
        vTaskDelay(10);
    }

    // Print summary lines showing the file, dir, and size totals.
    LOGF("\n%4u File(s),%10u bytes total\n%4u Dir(s)",
                ui32FileCount, ui32TotalSize, ui32DirCount);

    // Get the free space.
    res = hello_fs_getfree("/", (DWORD *)&ui32TotalSize, &psFatFs);

    // Check for error and return if there is a problem.
    if(res != FR_OK)
    {
        return((int)res);
    }

    // Display the amount of free space that was calculated.
    LOGF(", %10uK bytes free\n", (ui32TotalSize *
                                        psFatFs->csize / 2));

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
FRESULT cd( char * path ) {
    uint_fast8_t ui8Idx;
    FRESULT res;

    // Copy the current working path into a temporary buffer so it can be
    // manipulated.
    strcpy(path_buff, cwd_buff);

    // If the first character is /, then this is a fully specified path, and it
    // should just be used as-is.
    if(path[0] == '/')
    {
        // Make sure the new path is not bigger than the cwd buffer.
        if(strlen(path) + 1 > sizeof(cwd_buff))
        {
            LOGI("Resulting path name is too long\n");
            return(FR_INVALID_NAME);
        }

        // If the new path name (in argv[1])  is not too long, then copy it
        // into the temporary buffer so it can be checked.
        else
        {
            strncpy(path_buff, path, sizeof(path_buff));
        }
    }
    // If the argument is .. then attempt to remove the lowest level on the
    // CWD.
    else if(!strcmp(path, ".."))
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
        if(strlen(path_buff) + strlen(path) + 1 + 1 > sizeof(cwd_buff))
        {
            LOGI("Resulting path name is too long\n");
            return(FR_INVALID_NAME);
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
            strcat(path_buff, path);
        }
    }

    // At this point, a candidate new directory path is in chTmpBuf.  Try to
    // open it to make sure it is valid.
    res = hello_fs_opendir(&fsdirobj,path_buff);

    // If it can't be opened, then it is a bad path.  Inform the user and
    // return.
    if(res != FR_OK)
    {
		return ( res);
	}

    strncpy(cwd_buff,path_buff, sizeof(cwd_buff));
    return(FR_OK);
}


int
Cmd_cd(int argc, char *argv[]) {
	return cd( argv[1] );
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
	LOGF("%s\n", cwd_buff);
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
        LOGI("Resulting path name is too long\n");
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
    UINT ui16BytesRead;

    if(global_filename( argv[1] ))
    {
    	return 1;
    }

    res = hello_fs_open(&file_obj, path_buff, FA_READ);
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
        res = hello_fs_read(&file_obj, path_buff, 4,
                         &ui16BytesRead);

        // If there was an error reading, then print a newline and return the
        // error to the user.
        if(res != FR_OK)
        {
        	LOGF("\n");
            return((int)res);
        }
        // Null terminate the last block that was read to make it a null
        // terminated string that can be used with printf.
        path_buff[ui16BytesRead] = 0;

        // Print the last chunk of the file that was received.
        LOGF("%s", path_buff);

    }
    while(ui16BytesRead == 4);

    hello_fs_close( &file_obj );

    LOGF("\n");
    return(0);
}

int
Cmd_write_audio(char *argv[])
{
    FRESULT res;

    UINT bytes = 0;
	UINT bytes_written = 0;
	UINT bytes_to_write = strlen(argv[1])+1;

    if(global_filename( "TXBUF"))
    {
    	return 1;
    }
    LOGF("print");
    // Open the file for reading.
    //res = hello_fs_open(&file_obj, path_buff, FA_CREATE_NEW|FA_WRITE);
    res = hello_fs_open(&file_obj, path_buff, FA_WRITE);
    hello_fs_stat( path_buff, &file_info );

    if( file_info.fsize != 0 )
        res = hello_fs_lseek(&file_obj, file_info.fsize );

    do {
		res = hello_fs_write( &file_obj, argv[1]+bytes_written, bytes_to_write-bytes_written, &bytes );
		bytes_written+=bytes;
    } while( bytes_written < bytes_to_write );

    res = hello_fs_close( &file_obj );

    if(res != FR_OK)
    {
        return((int)res);
    }
    return(0);
}

int
Cmd_write(int argc, char *argv[])
{
	LOGF("Cmd_write\n");

	UINT bytes = 0;
	UINT bytes_written = 0;
	UINT bytes_to_write = strlen(argv[2]) + 1;

    if(global_filename( argv[1] ))
    {
    	return 1;

    }


    // Open the file for writing.
    FRESULT res = hello_fs_open(&file_obj, path_buff, FA_CREATE_NEW|FA_WRITE|FA_OPEN_ALWAYS);
    LOGF("res :%d\n",res);

    if(res != FR_OK && res != FR_EXIST){
    	LOGF("File open %s failed: %d\n", path_buff, res);
    	return res;
    }

    hello_fs_stat( path_buff, &file_info );

    if( file_info.fsize != 0 )
        res = hello_fs_lseek(&file_obj, file_info.fsize );

    do {
		res = hello_fs_write( &file_obj, argv[2]+bytes_written, bytes_to_write-bytes_written, &bytes );
		bytes_written+=bytes;
		LOGF("bytes written: %d\n", bytes_written);

    } while( bytes_written < bytes_to_write );

    res = hello_fs_close( &file_obj );

    if(res != FR_OK)
    {
        return((int)res);
    }
    return(0);
}

int
rm(char * file)
{
    FRESULT res;

    if(global_filename( file ))
    {
    	return 1;
    }

    res = hello_fs_unlink(path_buff);

    if(res != FR_OK)
    {
        return((int)res);
    }

    return(0);
}
int
Cmd_rm(int argc, char *argv[])
{
	return rm(argv[1]);
}

int mkdir( char * path ) {
    FRESULT res;

    if(global_filename( path ))
    {
    	return 1;
    }

    res = hello_fs_mkdir(path_buff);
    if(res != FR_OK)
    {
        return((int)res);
    }
    return(0);
}

int
Cmd_mkdir(int argc, char *argv[])
{
	return mkdir(argv[1]);
}


//begin download functions

//****************************************************************************
//
//! Create an endpoint for communication and initiate connection on socket.*/
//!
//! \brief Create connection with server
//!
//! This function opens a socket and create the endpoint communication with server
//!
//! \param[in]      DestinationIP - IP address of the server
//!
//! \return         socket id for success and negative for error
//
//****************************************************************************
int CreateConnection(unsigned long DestinationIP)
{
    SlSockAddrIn_t  Addr;
    int             Status = 0;
    int             AddrSize = 0;
    int             SockID = 0;

    timeval tv;

    Addr.sin_family = SL_AF_INET;
    Addr.sin_port = sl_Htons(80);
    Addr.sin_addr.s_addr = sl_Htonl(DestinationIP);

    AddrSize = sizeof(SlSockAddrIn_t);

    SockID = socket(SL_AF_INET,SL_SOCK_STREAM, 0); //todo secure socket
    ASSERT_ON_ERROR(SockID);

    tv.tv_sec = 2;             // Seconds
    tv.tv_usec = 0;           // Microseconds. 10000 microseconds resolution
    setsockopt(SockID, SOL_SOCKET, SL_SO_RCVTIMEO, &tv, sizeof(tv)); // Enable receive timeout

    SlSockNonblocking_t enableOption;
    enableOption.NonBlockingEnabled = 1;
    sl_SetSockOpt(SockID,SL_SOL_SOCKET,SL_SO_NONBLOCKING, (_u8 *)&enableOption,sizeof(enableOption)); // Enable/disable nonblocking mode

    do {
		vTaskDelay(100);
		Status = connect(SockID, ( SlSockAddr_t *)&Addr, AddrSize);
	} 	while( SL_ERROR_BSD_EALREADY == Status );

    if( Status < 0 )
    {
        /* Error */
        close(SockID);
        ASSERT_ON_ERROR(Status);
    }
    return SockID;
}

//****************************************************************************
//
//!
//! \brief Convert hex to decimal base
//!
//! \param[in]      ptr - pointer to string containing number in hex
//!
//! \return         number in decimal base
//!
//
//****************************************************************************
signed long hexToi(unsigned char *ptr)
{
    unsigned long result = 0;
    int idx = 0;
    unsigned int len = strlen((const char *)ptr);

    // convert characters to upper case
    for(idx = 0; ptr[idx] != '\0'; ++idx)
    {
        if( (ptr[idx] >= 'a') &&
            (ptr[idx] <= 'f') )
        {
            /* Change case - ASCII 'a' = 97, 'A' = 65 => 97-65 = 32 */
            ptr[idx] -= 32;
        }
    }

    for(idx = 0; ptr[idx] != '\0'; ++idx)
    {
        if(ptr[idx] >= '0' && ptr[idx] <= '9')
        {
            /* Converting '0' to '9' to their decimal value */
            result += (ptr[idx] - '0') * (1 << (4 * (len - 1 - idx)));
        }
        else if(ptr[idx] >= 'A' && ptr[idx] <= 'F')
        {
            /* Converting hex 'A' to 'F' to their decimal value */
            /* .i.e. 'A' - 55 = 10, 'F' - 55 = 15 */
            result += (ptr[idx] - 55) * (1 << (4 * (len -1 - idx)));
        }
        else
        {
            ASSERT_ON_ERROR(-1);
        }
    }

    return result;
}

//****************************************************************************
//
//! \brief Obtain the file from the server
//!
//!  This function requests the file from the server and save it on serial flash.
//!  To request a different file for different user needs to modify the
//!  PREFIX_BUFFER and POST_BUFFER macros.
//!
//! \param[in]      None
//!
//! \return         0 for success and negative for error
//

//download www.ti.com dl /lit/er/swrz044b/swrz044b.pdf
//download img4.wikia.nocookie.net al __cb20130206084237/disney/images/e/eb/Aladdin_-_A_Whole_New_World_%281%29.gif

//****************************************************************************
#include "stdlib.h"

typedef enum {
	SERIAL_FLASH,
	SD_CARD,
} storage_dev_t;

#include "crypto.h"

int GetData(char * filename, char* url, char * host, char * path, storage_dev_t storage)
{
    int           transfer_len = 0;
    UINT          r = 0;
    int           retry;
    unsigned char *pBuff = 0;
    UINT          recv_size = 0;

    long          fileHandle = -1;

    LOGI("Start downloading the file\r\n");

    unsigned char * g_buff = pvPortMalloc( MAX_BUFF_SIZE );
    assert( g_buff );

    memset(g_buff, 0, MAX_BUFF_SIZE);

    // Puts together the HTTP GET string.
    usnprintf( (char *)g_buff, MAX_BUFF_SIZE, "%s%s%s%s%s",
               PREFIX_BUFFER, url, POST_BUFFER_1, host, POST_BUFFER_2);

    assert( strlen((char *)g_buff) > 1 );

    LOGI("sent\r\n%s\r\n", g_buff );

    // Send the HTTP GET string to the opened TCP/IP socket.
    transfer_len = send(dl_sock, g_buff, strlen((const char *)g_buff), 0);

    if (transfer_len <= 0)
    {
        // error
    	LOGW("Sending error %d\r\n",transfer_len);
        ASSERT_ON_ERROR(-1);
    }

    memset(g_buff, 0, MAX_BUFF_SIZE);

    // get the reply from the server in buffer.
    retry = 0;
    do {
        vTaskDelay(500);
        transfer_len = recv(dl_sock, &g_buff[0], MAX_BUFF_SIZE, 0);
        if(++retry > SOCK_RETRY){
            break;
        }
        if(transfer_len < 0 ){
            vTaskDelay(500);
        }
    }while(transfer_len == EAGAIN);

    if(transfer_len <= 0){
        LOGW("Download error %d\r\n",transfer_len);
        ASSERT_ON_ERROR(-1);
    }
    //LOGI("recv:\r\n%s\r\n", g_buff ); don't echo binary

    // Check for 404 return code
    if(strstr((const char *)g_buff, HTTP_FILE_NOT_FOUND) != 0)
    {
        LOGW("HTTP_FILE_NOT_FOUND\r\n");
        ASSERT_ON_ERROR(HLO_HTTP_ERR);
    }
    if(strstr((const char *)g_buff, HTTP_FORBIDDEN) != 0)
    {
        LOGW("HTTP_FORBIDDEN\r\n");
        ASSERT_ON_ERROR(HLO_HTTP_ERR);
    }

    // if not "200 OK" return error
    if(strstr((const char *)g_buff, HTTP_STATUS_OK) == 0)
    {
        LOGW("UNKNOWN HTTP CODE!\r\n");
        LOGW("%s", g_buff);
        ASSERT_ON_ERROR(-1);
    }

    // check if content length is transfered with headers
    pBuff = (unsigned char *)strstr((const char *)g_buff, HTTP_CONTENT_LENGTH);
    if(pBuff != 0)
    {
    	char *p = (char*)pBuff;
		p += strlen(HTTP_CONTENT_LENGTH)+1;
		recv_size = atoi(p);

		if(recv_size <= 0) {
			vPortFree(g_buff);
			return -1;
		}
    }

    // Check if data is chunked
    pBuff = (unsigned char *)strstr((const char *)g_buff, HTTP_TRANSFER_ENCODING);
    if(pBuff != 0)
    {
        pBuff += strlen(HTTP_TRANSFER_ENCODING);
        while(*pBuff == 32)
            pBuff++;

        if(memcmp(pBuff, HTTP_ENCODING_CHUNKED, strlen(HTTP_ENCODING_CHUNKED)) == 0)
        {
            recv_size = 0;
        }
    }
    else
    {
        // Check if connection will be closed by after sending data
        // In this method the content length is not received and end of
        // connection marks the end of data
        pBuff = (unsigned char *)strstr((const char *)g_buff, HTTP_CONNECTION);
        if(pBuff != 0)
        {
            pBuff += strlen(HTTP_CONNECTION);
            while(*pBuff == 32)
                pBuff++;

            if(memcmp(pBuff, HTTP_ENCODING_CHUNKED, strlen(HTTP_CONNECTION_CLOSE)) == 0)
            {
                // not supported
                ASSERT_ON_ERROR(-1);
            }
        }
    }

    // "\r\n\r\n" marks the end of headers
    pBuff = (unsigned char *)strstr((const char *)g_buff, HTTP_END_OF_HEADER);
    if(pBuff == 0)
    {
        ASSERT_ON_ERROR(-1);
    }
    // Increment by 4 to skip "\r\n\r\n"
    pBuff += 4;

    // Adjust buffer data length for header size
    if( (pBuff - g_buff) < transfer_len ) {
    	transfer_len -= (pBuff - g_buff);
    } else {
    	transfer_len = 0;
    	vPortFree(g_buff);
    	return -1;
    }

    FRESULT res = FR_OK;

	if (storage == SD_CARD) {

		if(path == NULL)
		{
			LOGI("Path not provided, downloading to root\n");
			cd("/");
		}
		else
		{

			if(hello_fs_stat(path, NULL))
			{
				// Path doesn't exist, create directory
				res = (FRESULT) mkdir(path);
				if(res != FR_OK && res != FR_EXIST)
				{
					LOGE("mkdir fail: %d\n", res);
					cd("/");
					vPortFree(g_buff);
					return -1;
				}

			}

			res = cd(path);
			if(res)
			{
				LOGE("CD fail: %d\n", res);
				cd("/");
				vPortFree(g_buff);
				return -1;
			}
		}

		/* Open file to save the downloaded file */
		if (global_filename(filename)) {
			cd("/");
			vPortFree(g_buff);
			return -1;
		}
		// Open the file for writing.
		res = hello_fs_open(&file_obj, path_buff,
				FA_CREATE_ALWAYS | FA_WRITE );
		LOGI("res :%d\n", res);

		if (res != FR_OK && res != FR_EXIST) {
			LOGI("File open %s failed: %d\n", path_buff, res);
			cd("/");
			vPortFree(g_buff);
			return res;
		}
	} else if( storage == SERIAL_FLASH ) {
	    /* Open file to save the downloaded file */
	    unsigned long Token = 0;
	    strcpy(path_buff, path);
	    strcat(path_buff, filename);

	    long lRetVal = sl_FsOpen((unsigned char*)path_buff,
	    		SL_FS_WRITE, &Token, &fileHandle);
	    if(lRetVal < 0)
	    {
	        // File Doesn't exit create a new of 256 KB file
	        lRetVal = sl_FsOpen((unsigned char*)path_buff, \
	                           FS_MODE_OPEN_CREATE(256*1024, \
	                           SL_FS_FILE_OPEN_FLAG_COMMIT|SL_FS_FILE_PUBLIC_WRITE),
	                           &Token, &fileHandle);
			if (lRetVal < 0) {
				vPortFree(g_buff);
				return (lRetVal);
            }
		}
	    LOGI("opening %s\n", path_buff);
	}
    uint32_t total = recv_size;
    int percent = -1;

    //move the data back to the front of the buffer
    memcpy( g_buff, pBuff, transfer_len );
	//DISP( "begin %d\n", transfer_len);
    while (recv_size > 0)
    {
        int retries = 0;
        int recv_res = EAGAIN;
		while( ++retries < 1000 && transfer_len < MAX_BUFF_SIZE && recv_res == SL_EAGAIN ) {
			recv_res = recv(dl_sock, g_buff + transfer_len, MAX_BUFF_SIZE - transfer_len, 0);
			//DISP( "r %d ", recv_res);
			if( recv_res > 0 ) {
				transfer_len += recv_res;
			}
			if( recv_res == SL_EAGAIN ) {
				vTaskDelay(500);
			}
		}
		//DISP( " %d %d\n", transfer_len, recv_size);

		if( transfer_len == 0 ) {
        	LOGI("recv fail %d %d\r\n", recv_res, transfer_len );
        	goto failure;
		}
        pBuff = g_buff;

		MAP_WatchdogIntClear(WDT_BASE); //clear wdt

		// write data on the file
		if (storage == SD_CARD) {
			res = hello_fs_write(&file_obj, pBuff, transfer_len, &r);
		} else if (storage == SERIAL_FLASH) {
			//write to serial flash file
			r = sl_FsWrite(fileHandle, total - recv_size,
					(unsigned char *) pBuff, transfer_len);
		}

		if (r != transfer_len )
		{
			LOGE("failed to write %d %d\n",r,transfer_len);
			goto failure;
		}
		//LOGI("wrote:  %d %d\r\n", r, res); spamspamspam
		bytesReceived += transfer_len;
		recv_size -= transfer_len;

		if (100 - 100 * recv_size / total != percent) {
			percent = 100 - 100 * recv_size / total;
			LOGI("DL %d %d\r\n", recv_size, percent);
		}

        memset(g_buff, 0, MAX_BUFF_SIZE);
        transfer_len = 0;
    }

    // If user file has checksum which can be used to verify the temporary
    // file then file should be verified
    // In case of invalid file (FILE_NAME) should be closed without saving to
    // recover the previous version of file
    //
	LOGI(" successful file\r\n" );

	if (storage == SD_CARD) {
		/* Save and close file */
		res = hello_fs_close( &file_obj );

		if(res != FR_OK)
		{
			cd( "/" );
			vPortFree(g_buff);
			return((int)res);
		}
	} else if (storage == SERIAL_FLASH) {
		if( strstr(filename, "ucf") != 0) {
			uint8_t sig[] = {
					0x75, 0x20, 0xCD, 0x1B, 0xE3, 0xCD, 0x34, 0xC1, 0x40, 0x57, 0x47, 0x36, 0xFA, 0xC0, 0x99,
					0xE7, 0x4A, 0xFF, 0x0B, 0x91, 0x70, 0xEC, 0xD8, 0xDA, 0xF3, 0xFA, 0x58, 0x5D, 0x0C, 0x65,
					0x94, 0x23, 0x12, 0xB1, 0xA6, 0xAF, 0x03, 0xEE, 0x6A, 0xB4, 0x25, 0x3A, 0x2F, 0xBB, 0x2B,
					0x0E, 0x36, 0x91, 0x87, 0x05, 0x14, 0x53, 0xA6, 0x4E, 0xA1, 0xA6, 0x29, 0x46, 0x47, 0x3F,
					0x0D, 0xEE, 0xB4, 0x26, 0x39, 0xB9, 0x7D, 0xAF, 0xA6, 0x80, 0x81, 0x7B, 0x2D, 0x83, 0x51,
					0xF3, 0xDE, 0x22, 0xD9, 0x75, 0x15, 0xF2, 0xB1, 0x06, 0xF9, 0x06, 0x26, 0x79, 0x0B, 0xE4,
					0x3C, 0x49, 0x32, 0xB1, 0x5A, 0x12, 0x79, 0xA3, 0x59, 0xD7, 0xA7, 0xAC, 0x61, 0xEB, 0x86,
					0xDC, 0x6A, 0x7D, 0x06, 0xD1, 0xFB, 0x02, 0x94, 0xF0, 0x17, 0x22, 0xB8, 0xB0, 0xF8, 0x53,
					0x95, 0xDE, 0x47, 0x1E, 0x60, 0x40, 0x6A, 0x62, 0xDD, 0x2F, 0xB4, 0x27, 0xCE, 0xF8, 0xAE,
					0xFD, 0x78, 0xEC, 0x99, 0x11, 0xDD, 0x37, 0x4A, 0x09, 0x6B, 0x0B, 0x53, 0x6F, 0xDD, 0x48,
					0x0F, 0x7A, 0x10, 0x52, 0x45, 0x6A, 0x19, 0xE9, 0x09, 0xC6, 0x45, 0x64, 0xF2, 0xEB, 0xDF,
					0xBD, 0xEA, 0x46, 0xD9, 0x27, 0xDA, 0x4E, 0x7C, 0xDA, 0x51, 0x56, 0x57, 0xCD, 0xFE, 0x39,
					0x45, 0x2E, 0x9B, 0x48, 0x4D, 0x78, 0x98, 0x26, 0xD3, 0xED, 0xA0, 0xD1, 0xBF, 0x59, 0xEF,
					0xA7, 0x1B, 0x7A, 0xA7, 0x3C, 0xD8, 0x04, 0x93, 0x61, 0x2D, 0xDA, 0x9D, 0x72, 0xEE, 0xDC,
					0xE9, 0xAE, 0x88, 0xB1, 0x07, 0xFF, 0x74, 0xBA, 0x3E, 0x06, 0xF8, 0x25, 0xF1, 0x2C, 0x8C,
					0x30, 0x9B, 0xA7, 0x82, 0x1D, 0xE9, 0x74, 0x59, 0x30, 0xC8, 0x04, 0xBD, 0x0D, 0x6C, 0xDB,
					0x8A, 0xF2, 0xA2, 0x7C, 0x6E, 0x25, 0xD6, 0x53, 0x90, 0x9B, 0x42, 0x74, 0x14, 0xB3, 0xA2,0xFD};
			/* Save and close file */
			vPortFree(g_buff);
			return sl_FsClose(fileHandle, 0, sig, sizeof(sig));
		} else {
			/* Save and close file */
			vPortFree(g_buff);
			return sl_FsClose(fileHandle, 0, 0, 0);
		}
	}
	if (storage == SD_CARD) {
        cd( "/" );
	}
	vPortFree(g_buff);
	return 0;
failure:
	LOGE("FAIL\n");
	vPortFree(g_buff);
	if (storage == SD_CARD) {
        /* Close file without saving */
        res = hello_fs_close( &file_obj );

        if(res != FR_OK)
        {
        	cd( "/" );
            return((int)res);
        }
        hello_fs_unlink( path_buff );
        cd( "/" );
	} else if (storage == SERIAL_FLASH) {
        /* Close file without saving */
		sl_FsClose(fileHandle, 0, (unsigned char*) "A", 1);
	}
    return -1;
}

int file_exists( char * filename, char * path ) {
	cd(path);

    if(global_filename( filename ))
    {
    	return 1;
    }

    FRESULT res = hello_fs_open(&file_obj, path_buff, FA_READ);
    if(res != FR_OK)
    {
    	cd("/");
        return(0);
    }
    hello_fs_close( &file_obj );
	cd("/");
	return 1;
}

int download_file(char * host, char * url, char * filename, char * path, storage_dev_t storage ) {
	unsigned long ip;
	int r = gethostbyname((signed char*) host, strlen(host), &ip, SL_AF_INET);
	if (r < 0) {
		ASSERT_ON_ERROR(-1);
	}
    LOGF("host %s\ndownload ip %d.%d.%d.%d\n",
    			host,
                SL_IPV4_BYTE(ip,3),
                SL_IPV4_BYTE(ip,2),
                SL_IPV4_BYTE(ip,1),
                SL_IPV4_BYTE(ip,0));
	//LOGI("download <host> <filename> <url>\n\r");
	// Create a TCP connection to the Web Server
	dl_sock = CreateConnection(ip);
    LOGE("connect returns %d\n", dl_sock );
	if (dl_sock < 0) {
	    return dl_sock;
	}
	// Download the file, verify the file and replace the exiting file
	r = GetData(filename, url, host, path, storage);
	if (r != 0) {
		LOGF("GetData failed\n\r");
		close(dl_sock);
		return r;
	}

	r = close(dl_sock);

	return r;
}
//http://dropbox.com/on/drop/box/file.txt

//download dropbox.com somefile.txt /on/drop/box/file.txt /
int Cmd_download(int argc, char*argv[]) {
	return download_file( argv[1], argv[3], argv[2], argv[4], SD_CARD );
}

//end download functions
#include "sync_response.pb.h"
#include "stdbool.h"
#include "pb.h"
#include "pb_decode.h"

/******************************************************************************
   Image file names
*******************************************************************************/
#define IMG_BOOT_INFO           "/sys/mcubootinfo.bin"
#define IMG_FACTORY_DEFAULT     "/sys/mcuimg1.bin"
#define IMG_USER_1              "/sys/mcuimg2.bin"
#define IMG_USER_2              "/sys/mcuimg3.bin"

/******************************************************************************
   Image status
*******************************************************************************/
#define IMG_STATUS_TESTING      0x12344321
#define IMG_STATUS_TESTREADY    0x56788765
#define IMG_STATUS_NOTEST       0xABCDDCBA

/******************************************************************************
   Active Image
*******************************************************************************/
#define NUM_IMAGES			    3
#define IMG_ACT_FACTORY         0
#define IMG_ACT_USER1           1
#define IMG_ACT_USER2           2

//*****************************************************************************
// User image tokens
//*****************************************************************************
#define FACTORY_IMG_TOKEN       0x5555AAAA
#define USER_IMG_1_TOKEN        0xAA5555AA
#define USER_IMG_2_TOKEN        0x55AAAA55
#define USER_BOOT_INFO_TOKEN    0xA5A55A5A

#define DEVICE_IS_CC3101RS      0x18
#define DEVICE_IS_CC3101S       0x1B

/******************************************************************************
   Boot Info structure
*******************************************************************************/
typedef struct sBootInfo
{
  _u8  ucActiveImg;
  _u32 ulImgStatus;

  unsigned char sha[NUM_IMAGES][SHA1_SIZE];

  unsigned char shatop[1][SHA1_SIZE];//only use 0
}sBootInfo_t;

void mcu_reset();


//*****************************************************************************
//
//! Checks if the device is secure
//!
//! This function checks if the device is a secure device or not.
//!
//! \return Returns \b true if device is secure, \b false otherwise
//
//*****************************************************************************
#include "hw_ints.h"
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_gprcm.h"
#include "hw_common_reg.h"

static inline int IsSecureMCU()
{
  unsigned long ulChipId;

  ulChipId =(HWREG(GPRCM_BASE + GPRCM_O_GPRCM_EFUSE_READ_REG2) >> 16) & 0x1F;

  if((ulChipId != DEVICE_IS_CC3101RS) &&(ulChipId != DEVICE_IS_CC3101S))
  {
    //
    // Return non-Secure
    //
    return false;
  }

  //
  // Return secure
  //
  return true;
}


/* Save bootinfo on ImageCommit call */
static sBootInfo_t sBootInfo;

static _i32 _WriteBootInfo(sBootInfo_t *psBootInfo)
{
    _i32 hndl;
    unsigned long ulBootInfoToken = 0;
    unsigned long ulBootInfoCreateFlag;


    sl_FsDel((unsigned char *)IMG_BOOT_INFO,ulBootInfoToken);
    //
    // Initialize boot info file create flag
    //
    ulBootInfoCreateFlag  = FS_MODE_OPEN_CREATE(256, SL_FS_FILE_OPEN_FLAG_COMMIT|SL_FS_FILE_PUBLIC_WRITE);

    //
    // Check if its a secure MCU
    //
    if ( IsSecureMCU() )
    {
        LOGI("Boot info is secure mcu\r\n");
        ulBootInfoToken       = USER_BOOT_INFO_TOKEN;
        ulBootInfoCreateFlag  = SL_FS_FILE_OPEN_FLAG_COMMIT|SL_FS_FILE_OPEN_FLAG_SECURE|
            SL_FS_FILE_OPEN_FLAG_NO_SIGNATURE_TEST|
            SL_FS_FILE_PUBLIC_WRITE|SL_FS_FILE_OPEN_FLAG_VENDOR;
    }


	if (sl_FsOpen((unsigned char *)IMG_BOOT_INFO, SL_FS_WRITE, &ulBootInfoToken, &hndl)) {
		LOGI("error opening file, trying to create\n");

		if (sl_FsOpen((unsigned char *)IMG_BOOT_INFO, ulBootInfoCreateFlag, &ulBootInfoToken, &hndl)) {
            LOGE("Boot info open failed\n");
			return -1;
		}else{
            sl_FsWrite(hndl, 0, (_u8 *)psBootInfo, sizeof(sBootInfo_t));  // Dummy write, we don't care about the result
        }
	}
	if( 0 < sl_FsWrite(hndl, 0, (_u8 *)psBootInfo, sizeof(sBootInfo_t)) )
	{
		LOGI("WriteBootInfo: ucActiveImg=%d, ulImgStatus=0x%x\n\r", psBootInfo->ucActiveImg, psBootInfo->ulImgStatus);
	}
	sl_FsClose(hndl, 0, 0, 0);
    return 0;
}

static _i32 _ReadBootInfo(sBootInfo_t *psBootInfo)
{
    _i32 lFileHandle;
    _i32 status = -1;
    unsigned long ulBootInfoToken;
    //
    // Check if its a secure MCU
    //
    if ( IsSecureMCU() )
    {
      ulBootInfoToken       = USER_BOOT_INFO_TOKEN;
    }
    if( 0 == sl_FsOpen((unsigned char *)IMG_BOOT_INFO, SL_FS_MODE_OPEN_READ, &ulBootInfoToken, &lFileHandle) )
    {
        if( 0 < sl_FsRead(lFileHandle, 0, (_u8 *)psBootInfo, sizeof(sBootInfo_t)) )
        {
            status = 0;
            static bool printed = false;
            if( !printed ) {
            	LOGI("ReadBootInfo: ucActiveImg=%d, ulImgStatus=0x%x\n\r", psBootInfo->ucActiveImg, psBootInfo->ulImgStatus);
            	printed = true;
            }
        }
        sl_FsClose(lFileHandle, 0, 0, 0);
    }
    return status;
}
static _i32 _McuImageGetNewIndex(void)
{
    _i32 newImageIndex;

    /* Assume sBootInfo is alrteady filled in init time (by sl_extlib_FlcCommit) */
    switch(sBootInfo.ucActiveImg)
    {
        case IMG_ACT_USER1:
            newImageIndex = IMG_ACT_USER2;
            break;

        case IMG_ACT_USER2:
        default:
            newImageIndex = IMG_ACT_USER1;
            break;
    }
    LOGI("_McuImageGetNewIndex: active image is %d, return new image %d \n\r", sBootInfo.ucActiveImg, newImageIndex);

    return newImageIndex;
}
#include "wifi_cmd.h"
#include "FreeRTOS.h"
#include "task.h"
#include "top_board.h"

int wait_for_top_boot(unsigned int timeout);
int send_top(char *, int);

bool is_test_boot() {
	static bool read_bootinfo = false;

	if( !read_bootinfo ) {
		_ReadBootInfo(&sBootInfo);
		read_bootinfo = true;
	}
	/* Check only on status TESTING */
	return IMG_STATUS_TESTING == sBootInfo.ulImgStatus;
}

void boot_commit_ota() {
    _ReadBootInfo(&sBootInfo);
    /* Check only on status TESTING */
    if( IMG_STATUS_TESTING == sBootInfo.ulImgStatus )
	{
        DISP("\r\n-----------------------\r\n");
		LOGI("Booted in testing mode\r\n");
        DISP("\r\n-----------------------\r\n");
        if( !verify_top_update() ){
            LOGI("Updating top board\r\n");
            send_top("dfu", strlen("dfu"));
            if(wait_for_top_boot(60000)){
                LOGI("Top board update success\r\n");
				//delete update on success
				sl_FsDel( "/top/update.bin", 0);
            }else{
                LOGE("Top board update failed\r\n");
                //FORCE boot into factory next time
            }
        }else{
            LOGW("TOP checksum error, skipping update\r\n");
        }
		sBootInfo.ulImgStatus = IMG_STATUS_NOTEST;
		sBootInfo.ucActiveImg = (sBootInfo.ucActiveImg == IMG_ACT_USER1)?
								IMG_ACT_USER2:
								IMG_ACT_USER1;
        _WriteBootInfo(&sBootInfo);
		//send_top("rst ", sizeof("rst "));
		mcu_reset();
	}
}

void reset_to_factory_fw() {
	_ReadBootInfo(&sBootInfo);
	sBootInfo.ulImgStatus = IMG_STATUS_NOTEST;
	sBootInfo.ucActiveImg = IMG_ACT_FACTORY;
	_WriteBootInfo(&sBootInfo);
	mcu_reset();
}

#include "wifi_cmd.h"
int Cmd_version(int argc, char *argv[]) {
	LOGF( "ver: %08x\nimg: %d\nstatus: %x\n", KIT_VER, sBootInfo.ucActiveImg, sBootInfo.ulImgStatus );
	return 0;
}

bool _decode_string_field(pb_istream_t *stream, const pb_field_t *field, void **arg);
void free_download_info(SyncResponse_FileDownload * download_info) {
	char * filename=NULL, * url=NULL, * host=NULL, * path=NULL, * serial_flash_path=NULL, * serial_flash_name=NULL;

	filename = download_info->sd_card_filename.arg;
	path = download_info->sd_card_path.arg;
	url = download_info->url.arg;
	host = download_info->host.arg;
	serial_flash_name = download_info->serial_flash_filename.arg;
	serial_flash_path = download_info->serial_flash_path.arg;
	if( filename ) {
		vPortFree(filename);
	}
	if( url ) {
		vPortFree(url);
	}
	if( host ) {
		vPortFree(host);
	}
	if( path ) {
		vPortFree(path);
	}
	if( serial_flash_name ) {
		vPortFree(serial_flash_name);
	}
	if( serial_flash_path ) {
		vPortFree(serial_flash_path);
	}
}

typedef enum {
	sha_file_create=0,
	sha_file_delete,
	sha_file_get_sha
}update_sha_t;
void update_file_download_status(bool is_pending);
uint32_t update_sha_file(char* path, char* original_filename, update_sha_t option, uint8_t* sha, bool ovwr);
xQueueHandle download_queue = 0;

void file_download_task( void * params ) {
    SyncResponse_FileDownload download_info;
    unsigned char top_sha_cache[SHA1_SIZE];
    int top_need_dfu = 0;
    for(;;) while (xQueueReceive(download_queue, &(download_info), portMAX_DELAY)) {
        char * filename=NULL, * url=NULL, * host=NULL, * path=NULL, * serial_flash_path=NULL, * serial_flash_name=NULL;

        filename = download_info.sd_card_filename.arg;
        path = download_info.sd_card_path.arg;
        url = download_info.url.arg;
        host = download_info.host.arg;
        serial_flash_name = download_info.serial_flash_filename.arg;
        serial_flash_path = download_info.serial_flash_path.arg;

        if( strlen(filename) == 0 && strlen(serial_flash_name) == 0 ) {
            LOGE( "no file name!\n");
            goto next_one;
        }

        if( filename ) {
            LOGI( "ota - filename: %s\n", filename);
        }
        if( url ) {
            LOGI( "ota - url: %s\n",url);
        }
        if( host ) {
            LOGI( "ota - host: %s\n",host);
        }
        if( path ) {
            LOGI( "ota - path: %s\n",path);
        }
        if( serial_flash_path ) {
            LOGI( "ota - serial_flash_path: %s\n",serial_flash_path);
        }
        if( serial_flash_name ) {
            LOGI( "ota - serial_flash_name: %s\n",serial_flash_name);
        }
        if( download_info.has_copy_to_serial_flash ) {
            LOGI( "ota - copy_to_serial_flash: %s\n",download_info.copy_to_serial_flash);
        }

        if (filename && url && host && path) {
            int exists = 0;
            if (file_exists(filename, path)) {
                LOGI("ota - file exists, overwriting\n");
                exists = 1;
            }

            if(global_filename( filename ))
            {
                goto end_download_task;
            }
            if( exists ) {
                hello_fs_unlink(path_buff);
            }

            if (download_info.has_copy_to_serial_flash
                    && download_info.copy_to_serial_flash && serial_flash_name
                    && serial_flash_path) {
                //download it!
                if (strstr(serial_flash_name, "mcuimgx") != 0 )
                {
                    _ReadBootInfo(&sBootInfo);
                    serial_flash_name[6] = (_u8)_McuImageGetNewIndex() + '1'; /* mcuimg1 is for factory default, mcuimg2,3 are for OTA updates */
                    LOGI("MCU image name converted to %s \n", serial_flash_name);
                }

                int retries = 0;
                int dl_ret = -1;
                while(dl_ret != 0) {
                    dl_ret = download_file(host, url, serial_flash_name, serial_flash_path, SERIAL_FLASH);
                    if( dl_ret == HLO_HTTP_ERR ) {
                        goto next_one;
                    }
                    if( ++retries > HTTP_RETRIES ) {
                        goto end_download_task;
                    }
                }

                char buf[64];
                strncpy( buf, serial_flash_path, 64 );
                strncat(buf, serial_flash_name, 64 );

                if (strcmp(buf, "/top/update.bin") == 0) {
                    if (download_info.has_sha1) {
                        memcpy(top_sha_cache, download_info.sha1.bytes, SHA1_SIZE );
                        if( sf_sha1_verify((char *)download_info.sha1.bytes, buf)){
                            LOGW("Top DFU download failed\r\n");
                            top_need_dfu = 0;
                            goto end_download_task;
                        }else{
                            top_need_dfu = 1;
                        }
                    }
                }
                LOGI("done, closing\n");
			} else {

				int retries = 0;

				// Set file download pending for download manager
				update_file_download_status(true);

				// Delete corresponding SHA file if exists
				// delete before downloading the file, so that if the download fails midway,
				// the device won't be left with a corrupt file and a valid sha file.
				update_sha_file(path, filename, sha_file_delete,NULL, false );

				int dl_ret = -1;
                while(dl_ret != 0) {
                    dl_ret = download_file(host, url, filename, path, SD_CARD);
                    if( dl_ret == HLO_HTTP_ERR ) {
                        goto next_one;
                    }
                    if( ++retries > HTTP_RETRIES ) {
                        goto end_download_task;
                    }
                }

				if (download_info.has_sha1) {

					// SHA verify for SD card files
					FRESULT res = FR_OK;

					res = cd(path);
					if(res)
					{
						LOGE("CD fail: %d\n", res);
						cd("/");
						goto end_download_task;
					}

					/* Open file to save the downloaded file */
					if (global_filename(filename)) {
						cd("/");
						goto end_download_task;
					}

					//LOGI("Path buf : %s \n", path_buff);

					if( update_sha_file(path, filename, sha_file_create, download_info.sha1.bytes, true)){
						LOGW("SD card download fail\r\n");
						cd("/");
						goto end_download_task;
					}

					cd("/");
					LOGI("SD card download success \r\n");
				}

				// Clear file download status
				update_file_download_status(false);

            }
        }
        if (download_info.has_reset_application_processor
                && download_info.reset_application_processor) {
            _ReadBootInfo(&sBootInfo);
            if (download_info.has_sha1) {
                static unsigned char full_path[128];
                int res;
                strncpy((char*)full_path, serial_flash_path, sizeof(full_path)-1);
                full_path[sizeof(full_path)-1] = 0;
                strncat((char*)full_path, serial_flash_name, sizeof(full_path) - strlen((char*)full_path) - 1);

                res = sf_sha1_verify((char *)download_info.sha1.bytes, (char *)full_path);

                if(res){
                    goto end_download_task;
                }else{
                    if(top_need_dfu){
                        LOGI("Writing topboard SHA\r\n");
                        /*
                         *memcpy(sBootInfo.shatop[0], top_sha_cache, SHA1_SIZE );
                         */
                        set_top_update_sha((char*)top_sha_cache, 0);
                    }
                    LOGI("change image status to IMG_STATUS_TESTREADY\n\r");
                    sBootInfo.ulImgStatus = IMG_STATUS_TESTREADY;
                    memcpy(sBootInfo.sha[_McuImageGetNewIndex()], download_info.sha1.bytes, SHA1_SIZE );
                    //sBootInfo.ucActiveImg this is set by boot loader
                    _WriteBootInfo(&sBootInfo);
                    mcu_reset();
                }
            } else {
                LOGI( "no download SHA on fw!\n");
                goto end_download_task;
            }
        }
        if( download_info.has_reset_network_processor && download_info.reset_network_processor ) {
            LOGI( "reset nwp\n" );
            nwp_reset();
        }
next_one:
        free_download_info(&download_info);
        continue;

        //what if we get the next set before the current set finishes? How do we know which set we're on? (doesn't matter, it will just overflow the queue)
        //what if there's an error on some but not all the files? (start over)

end_download_task: //there was an error
		// Clear file download status
		update_file_download_status(false);
        while (xQueueReceive(download_queue, &download_info, 10)) {
            free_download_info(&download_info);
        }
    }
}

void init_download_task( int stack ){
	download_queue = xQueueCreate(10, sizeof(SyncResponse_FileDownload));
	xTaskCreate(file_download_task, "file_download_task", stack, NULL, 2, NULL);
}
#include "ble_proto.h"
bool _on_file_download(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
	SyncResponse_FileDownload download_info;

	download_info.sd_card_filename.funcs.decode = _decode_string_field;
	download_info.sd_card_filename.arg = NULL;

	download_info.sd_card_path.funcs.decode = _decode_string_field;
	download_info.sd_card_path.arg = NULL;

	download_info.url.funcs.decode = _decode_string_field;
	download_info.url.arg = NULL;

	download_info.host.funcs.decode = _decode_string_field;
	download_info.host.arg = NULL;

	download_info.serial_flash_filename.funcs.decode = _decode_string_field;
	download_info.serial_flash_filename.arg = NULL;

	download_info.serial_flash_path.funcs.decode = _decode_string_field;
	download_info.serial_flash_path.arg = NULL;

	LOGI("ota - parsing\n" );
	if( !pb_decode(stream,SyncResponse_FileDownload_fields,&download_info) ) {
		LOGI("ota - parse fail \n" );
		free_download_info( &download_info );
		return false;
	}

    _ReadBootInfo(&sBootInfo);
    /* Check only on status TESTING */
    if( IMG_STATUS_TESTING == sBootInfo.ulImgStatus ){
		LOGI("ota - ignoring due to test boot \n" );
		free_download_info( &download_info );
		return true;
	}

#if 0 //not reliable
	if(  get_ble_mode() != BLE_CONNECTED ) {
		LOGI("ota - ble active \n" );
		free_download_info( &download_info );
		return true;
	}
#endif
	if( download_queue ) {
		if( xQueueSend(download_queue, (void*)&download_info, 10) != pdPASS ) {
			free_download_info( &download_info );
		}
	}
	return true;
}
int sf_sha1_verify(const char * sha_truth, const char * serial_file_path){
    //compute the sha of the file..
#define minval( a,b ) a < b ? a : b

    unsigned char sha[SHA1_SIZE] = { 0 };
    static unsigned char buffer[128];
    SHA1_CTX sha1ctx;
    SHA1_Init(&sha1ctx);
    unsigned long tok = 0;
    long hndl, err, bytes, bytes_to_read;
    SlFsFileInfo_t info;
    //fetch path info
    LOGI( "computing SHA of %s\n", serial_file_path);
    sl_FsGetInfo((unsigned char*)serial_file_path, tok, &info);
    err = sl_FsOpen((unsigned char*)serial_file_path, SL_FS_READ, &tok, &hndl);
    if (err) {
        LOGI("error opening for read %d\n", err);
        return -1;
    }
    //compute sha
    bytes_to_read = info.Len;
    while (bytes_to_read > 0) {
        bytes = sl_FsRead(hndl, info.Len - bytes_to_read,
                buffer,
                (minval(sizeof(buffer),bytes_to_read)));
        SHA1_Update(&sha1ctx, buffer, bytes);
        bytes_to_read -= bytes;
    }
    sl_FsClose(hndl, 0, 0, 0);
    SHA1_Final(sha, &sha1ctx);
    //compare
    if (memcmp(sha, sha_truth, SHA1_SIZE) != 0) {
        LOGE( "fw update SHA did not match!\n");
        LOGI("Sha truth:      %02x ... %02x\r\n", sha_truth[0], sha_truth[SHA1_SIZE-1]);
        LOGI("Sha calculated: %02x ... %02x\r\n", sha[0], sha[SHA1_SIZE-1]);
        return -1;
    }

    LOGI( "SHA Match!\n");
    return 0;
}

bool send_to_download_queue(SyncResponse_FileDownload* data, TickType_t ticks_to_wait)
{
	if( download_queue )
	{
		if( xQueueSend(download_queue, (void*)data, ticks_to_wait) != pdPASS )
		{
			//free_file_sync_info( &download_info );
			return false;
		}
	}
	else
	{
		return false;
	}

	return true;

}

uint32_t get_free_space(uint32_t* free_space, uint32_t* total_mem)
{
    uint32_t ui32TotalSize;

    FRESULT res;
    FATFS *psFatFs;


    // Get the free space.
    res = hello_fs_getfree("/", (DWORD *)&ui32TotalSize, &psFatFs);

    // Check for error and return if there is a problem.
    if(res != FR_OK)
    {
        return((int)res);
    }

    *free_space = (ui32TotalSize * psFatFs->csize / 2);
    *total_mem = ((psFatFs->n_fatent-2) * psFatFs->csize / 2);

    // Display the amount of free space that was calculated.
    LOGF("%1uK bytes free of %uK bytes total\n",*free_space, *total_mem );

    return 0;

}

