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

#include "FreeRTOS.h"


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
            UARTprintf("Resulting path name is too long\n");
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
            UARTprintf("Resulting path name is too long\n");
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
    res = f_opendir(&fsdirobj,path_buff);

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
rm(char * file)
{
    FRESULT res;

    if(global_filename( file ))
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

    res = f_mkdir(path_buff);
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

#include "wlan.h"
#include "socket.h"
#include "simplelink.h"
#include "protocol.h"
#include "common.h"

#define PREFIX_BUFFER    "GET "
//#define POST_BUFFER      " HTTP/1.1\nAccept: text/html, application/xhtml+xml, */*\n\n"
#define POST_BUFFER_1  " HTTP/1.1\nHost:"
#define POST_BUFFER_2  "\nAccept: text/html, application/xhtml+xml, */*\n\n"

#define HTTP_FILE_NOT_FOUND    "404 Not Found" /* HTTP file not found response */
#define HTTP_STATUS_OK         "200 OK"  /* HTTP status ok response */
#define HTTP_CONTENT_LENGTH    "Content-Length:"  /* HTTP content length header */
#define HTTP_TRANSFER_ENCODING "Transfer-Encoding:" /* HTTP transfer encoding header */
#define HTTP_ENCODING_CHUNKED  "chunked" /* HTTP transfer encoding header value */
#define HTTP_CONNECTION        "Connection:" /* HTTP Connection header */
#define HTTP_CONNECTION_CLOSE  "close"  /* HTTP Connection header value */

#define DNS_RETRY           5 /* No of DNS tries */
#define HTTP_END_OF_HEADER  "\r\n\r\n"  /* string marking the end of headers in response */
#define SIZE_40K            40960  /* Serial flash file size 40 KB */

#define MAX_BUFF_SIZE      512

// Temporary file to store the downloaded file
#define TEMP_FILE_NAME "cc3000_module_temp.pdf"
// File on the serial flash to be replaced
#define FILE_NAME "test.pdf"


// Application specific status/error codes
typedef enum{
 /* Choosing this number to avoid overlap with host-driver's error codes */
    DEVICE_NOT_IN_STATION_MODE =1 ,
    DEVICE_START_FAILED = 2,
    INVALID_HEX_STRING = 3,
    TCP_RECV_ERROR = 4,
    TCP_SEND_ERROR = 5,
    FILE_NOT_FOUND_ERROR = 6,
    INVALID_SERVER_RESPONSE = 7,
    FORMAT_NOT_SUPPORTED = 8,
    FILE_OPEN_FAILED = 9,
    FILE_WRITE_ERROR = 10,
    INVALID_FILE = 11,
    SERVER_CONNECTION_FAILED =12,
    GET_HOST_IP_FAILED = 13,

    STATUS_CODE_MAX
} dl_status_codes;

unsigned char g_buff[MAX_BUFF_SIZE+1];
long bytesReceived = 0; // variable to store the file size
static int dl_sock = -1;


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

    Addr.sin_family = SL_AF_INET;
    Addr.sin_port = sl_Htons(80);
    Addr.sin_addr.s_addr = sl_Htonl(DestinationIP);

    AddrSize = sizeof(SlSockAddrIn_t);

    SockID = socket(SL_AF_INET,SL_SOCK_STREAM, 0); //todo secure socket
    ASSERT_ON_ERROR(SockID);

    Status = connect(SockID, ( SlSockAddr_t *)&Addr, AddrSize);
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
            ASSERT_ON_ERROR(INVALID_HEX_STRING);
        }
    }

    return result;
}

//****************************************************************************
//
//! \brief Calculate the file chunk size
//!
//! \param[in]      len - pointer to length of the data in the buffer
//! \param[in]      p_Buff - pointer to ponter of buffer containing data
//! \param[out]     chunk_size - pointer to variable containing chunk size
//!
//! \return         0 for success, -ve for error
//
//****************************************************************************
int GetChunkSize(int *len, unsigned char **p_Buff, unsigned long *chunk_size)
{
    int           idx = 0;
    unsigned char lenBuff[10];
    signed long r = -1;

    idx = 0;
    memset(lenBuff, 0, 10);
    while(*len >= 0 && **p_Buff != 13) /* check for <CR> */
    {
        if(*len == 0)
        {
            memset(g_buff, 0, sizeof(g_buff));
            *len = recv(dl_sock, g_buff, MAX_BUFF_SIZE, 0);

            UARTprintf( "chunked rx:\r\n%s\r\n", g_buff);
            if(*len <= 0)
                ASSERT_ON_ERROR(TCP_RECV_ERROR);

            *p_Buff = g_buff;
        }
        lenBuff[idx] = **p_Buff;
        idx++;
        (*p_Buff)++;
        (*len)--;
    }
    (*p_Buff) += 2; // skip <CR><LF>
    (*len) -= 2;
    r = hexToi(lenBuff);
    if(r < 0)
    {
        ASSERT_ON_ERROR(INVALID_HEX_STRING);
    }
    else
    {
        *chunk_size = r;
    }

    return 0;
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
#include "led_animations.h"
int GetData(char * filename, char* url, char * host, char * path)
{
    int           transfer_len = 0;
    WORD          r = 0;
    unsigned char *pBuff = 0;
    char          eof_detected = 0;
    unsigned long recv_size = 0;
    unsigned char isChunked = 0;

    UARTprintf("Start downloading the file\r\n");

    memset(g_buff, 0, sizeof(g_buff));

    // Puts together the HTTP GET string.
    strcpy((char *)g_buff, PREFIX_BUFFER);
    strcat((char *)g_buff, url);
    strcat((char *)g_buff, POST_BUFFER_1);
    strcat((char *)g_buff, host);
    strcat((char *)g_buff, POST_BUFFER_2);

    UARTprintf("sent\r\n%s\r\n", g_buff );

    // Send the HTTP GET string to the opened TCP/IP socket.
    transfer_len = send(dl_sock, g_buff, strlen((const char *)g_buff), 0);

    if (transfer_len < 0)
    {
        // error
        ASSERT_ON_ERROR(TCP_SEND_ERROR);
    }

    memset(g_buff, 0, sizeof(g_buff));

    // get the reply from the server in buffer.
    transfer_len = recv(dl_sock, &g_buff[0], MAX_BUFF_SIZE, 0);

    if(transfer_len <= 0)
    {
        ASSERT_ON_ERROR(TCP_RECV_ERROR);
    }

    UARTprintf("recv:\r\n%s\r\n", g_buff );


    // Check for 404 return code
    if(strstr((const char *)g_buff, HTTP_FILE_NOT_FOUND) != 0)
    {
        ASSERT_ON_ERROR(FILE_NOT_FOUND_ERROR);
    }

    // if not "200 OK" return error
    if(strstr((const char *)g_buff, HTTP_STATUS_OK) == 0)
    {
        ASSERT_ON_ERROR(INVALID_SERVER_RESPONSE);
    }

    // check if content length is transfered with headers
    pBuff = (unsigned char *)strstr((const char *)g_buff, HTTP_CONTENT_LENGTH);
    if(pBuff != 0)
    {
    	char *p = (char*)pBuff;
        // not supported
        ASSERT_ON_ERROR(FORMAT_NOT_SUPPORTED);

    	p += strlen(HTTP_CONTENT_LENGTH)+1;
    	recv_size = atoi(p);
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
            isChunked = 1;
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
                ASSERT_ON_ERROR(FORMAT_NOT_SUPPORTED);
            }
        }
    }

    // "\r\n\r\n" marks the end of headers
    pBuff = (unsigned char *)strstr((const char *)g_buff, HTTP_END_OF_HEADER);
    if(pBuff == 0)
    {
        ASSERT_ON_ERROR(INVALID_SERVER_RESPONSE);
    }
    // Increment by 4 to skip "\r\n\r\n"
    pBuff += 4;

    // Adjust buffer data length for header size
    if( (pBuff - g_buff) < transfer_len ) {
    	transfer_len -= (pBuff - g_buff);
    } else {
    	transfer_len = 0;
    	return -1;
    }

    // If data in chunked format, calculate the chunk size
    if(isChunked == 1)
    {
        r = GetChunkSize(&transfer_len, &pBuff, &recv_size);
    }
    FRESULT res;

	mkdir( path );
	cd( path );

    /* Open file to save the downloaded file */
    if(global_filename( filename ))
    {
    	cd( "/" );
    	return 1;
    }
    // Open the file for writing.
    res = f_open(&file_obj, path_buff, FA_CREATE_NEW|FA_WRITE|FA_OPEN_ALWAYS);
    UARTprintf("res :%d\n",res);

    if(res != FR_OK && res != FR_EXIST){
    	UARTprintf("File open %s failed: %d\n", path_buff, res);
    	cd( "/" );
    	return res;
    }
    uint32_t total = recv_size;
    int percent = 101-100*recv_size/total;
	play_led_progress_bar(132, 233, 4, 0);

    while (0 < transfer_len)
    {
    	if( 100-100*recv_size/total != percent ) {
    		percent = 100-100*recv_size/total;
    		UARTprintf("dl loop %d %d\r\n", recv_size, percent );
    		set_led_progress_bar( percent );
    	}

        // For chunked data recv_size contains the chunk size to be received
        if(recv_size <= transfer_len)
        {
            // write the recv_size
            res = f_write( &file_obj, pBuff, transfer_len, &r );

            UARTprintf("chunked 1 wrote:  %d %d\r\n", r, res);

            if(r < recv_size)
            {
                UARTprintf("Failed during writing the file, Error-code: %d\r\n", \
                            FILE_WRITE_ERROR);
                /* Close file without saving */
                res = f_close( &file_obj );

                if(res != FR_OK)
                {
                	cd( "/" );
        			stop_led_animation();
                    return((int)res);
                }
                f_unlink( path_buff );
                cd( "/" );
    			stop_led_animation();
                return r;
            }
            transfer_len -= recv_size;
            bytesReceived +=recv_size;
            pBuff += recv_size;
            recv_size = 0;

            if(isChunked == 1)
            {
                // if data in chunked format calculate next chunk size
                pBuff += 2; // 2 bytes for <CR> <LF>
                transfer_len -= 2;

                if(GetChunkSize(&transfer_len, &pBuff, &recv_size) < 0)
                {
                    // Error
                    break;
                }

                // if next chunk size is zero we have received the complete file
                if(recv_size == 0)
                {
                    eof_detected = 1;
                    break;
                }

                if(recv_size < transfer_len)
                {
                    // Code will enter this section if the new chunk size is
                    // less than the transfer size. This will the last chunk of
                    // file received
                    res = f_write( &file_obj, pBuff, transfer_len, &r );

                    UARTprintf("chunked 2 wrote:  %d %d\r\n", r, res);

                    if(r < recv_size)
                    {
                        UARTprintf("Failed during writing the file, " \
                                    "Error-code: %d\r\n", FILE_WRITE_ERROR);
                        /* Close file without saving */
                        res = f_close( &file_obj );

                        if(res != FR_OK)
                        {
                			stop_led_animation();
                        	cd( "/" );
                            return((int)res);
                        }
                        f_unlink( path_buff );
                        cd( "/" );
            			stop_led_animation();
                        return FILE_WRITE_ERROR;
                    }
                    transfer_len -= recv_size;
                    bytesReceived +=recv_size;
                    pBuff += recv_size;
                    recv_size = 0;

                    pBuff += 2; // 2bytes for <CR> <LF>
                    transfer_len -= 2;

                    // Calculate the next chunk size, should be zero
                    if(GetChunkSize(&transfer_len, &pBuff, &recv_size) < 0)
                    {
                        // Error
                        break;
                    }

                    // if next chunk size is non zero error
                    if(recv_size != 0)
                    {
                        // Error
                        break;
                    }
                    eof_detected = 1;
                    break;
                }
                else
                {
                    // write data on the file
                    res = f_write( &file_obj, pBuff, transfer_len, &r );

                    UARTprintf("chunked 3 wrote:  %d %d\r\n", r, res);

                    if(r < transfer_len)
                    {
                        UARTprintf("Failed during writing the file, " \
                                    "Error-code: %d\r\n", FILE_WRITE_ERROR);
                        /* Close file without saving */
                        res = f_close( &file_obj );

                        if(res != FR_OK)
                        {
                        	cd( "/" );
                			stop_led_animation();
                            return((int)res);
                        }
                        f_unlink( path_buff );
                        cd( "/" );
            			stop_led_animation();
                        return FILE_WRITE_ERROR;
                    }
                    recv_size -= transfer_len;
                    bytesReceived +=transfer_len;
                }
            }
            // complete file received exit
            if(recv_size == 0)
            {
                eof_detected = 1;
                break;
            }
        }
        else
        {
            // write data on the file
            res = f_write( &file_obj, pBuff, transfer_len, &r );

            //UARTprintf("wrote:  %d %d\r\n", r, res);

            if (r != transfer_len )
            {
                UARTprintf("Failed during writing the file, Error-code: " \
                            "%d\r\n", FILE_WRITE_ERROR);
                /* Close file without saving */
                res = f_close( &file_obj );

                if(res != FR_OK)
                {
                	cd( "/" );
        			stop_led_animation();
                    return((int)res);
                }
                f_unlink( path_buff );
    			stop_led_animation();
                return FILE_WRITE_ERROR;
            }
            bytesReceived +=transfer_len;
            recv_size -= transfer_len;
        }

        memset(g_buff, 0, sizeof(g_buff));

        transfer_len = recv(dl_sock, &g_buff[0], MAX_BUFF_SIZE, 0);
        //UARTprintf("rx:  %d\r\n", transfer_len);
        if(transfer_len <= 0) {
        	UARTprintf("TCP_RECV_ERROR\r\n" );
        	cd( "/" );
			stop_led_animation();
            return TCP_RECV_ERROR;
        }

        pBuff = g_buff;
    }

	stop_led_animation();

    //
    // If user file has checksum which can be used to verify the temporary
    // file then file should be verified
    // In case of invalid file (FILE_NAME) should be closed without saving to
    // recover the previous version of file
    //
    if(0 > transfer_len || eof_detected == 0)
    {
    	UARTprintf(" invalid file\r\n" );
        /* Close file without saving */
        res = f_close( &file_obj );

        if(res != FR_OK)
        {
        	cd( "/" );
            return((int)res);
        }
        f_unlink( path_buff );
        cd( "/" );
        return INVALID_FILE;
    }
    else
    {
    	UARTprintf(" successful file\r\n" );
        /* Save and close file */
        res = f_close( &file_obj );

        if(res != FR_OK)
        {
        	cd( "/" );
            return((int)res);
        }
    }
    cd( "/" );
    return 0;
}

int file_exists( char * filename, char * path ) {
	cd(path);

    if(global_filename( filename ))
    {
    	return 1;
    }

    FRESULT res = f_open(&file_obj, path_buff, FA_READ);
    if(res != FR_OK)
    {
    	cd("/");
        return(0);
    }
    f_close( &file_obj );
	cd("/");
	return 1;
}

int download_file(char * host, char * url, char * filename, char * path ) {
	unsigned long ip;
	int r = gethostbyname((signed char*) host, strlen(host), &ip, SL_AF_INET);
	if (r < 0) {
		ASSERT_ON_ERROR(GET_HOST_IP_FAILED);
	}
	//UARTprintf("download <host> <filename> <url>\n\r");
	// Create a TCP connection to the Web Server
	dl_sock = CreateConnection(ip);

	if (dl_sock < 0) {
		UARTprintf("Connection to server failed\n\r");
		return SERVER_CONNECTION_FAILED;
	} else {
		UARTprintf("Connection to server created successfully\r\n");
	}
	// Download the file, verify the file and replace the exiting file
	r = GetData(filename, url, host, path);
	if (r < 0) {
		UARTprintf("Device couldn't download the file from the server\n\r");
	}

	r = close(dl_sock);

	return r;
}
//http://dropbox.com/on/drop/box/file.txt

//download dropbox.com somefile.txt /on/drop/box/file.txt /
int Cmd_download(int argc, char*argv[]) {
	return download_file( argv[1], argv[3], argv[2], argv[4] );
}

//end download functions
#include "protobuf/SyncResponse.pb.h"
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
#define NUM_OTA_IMAGES			2
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

#define SHA_SIZE 32

/******************************************************************************
   Boot Info structure
*******************************************************************************/
typedef struct sBootInfo
{
  _u8  ucActiveImg;
  _u32 ulImgStatus;

  unsigned char sha[NUM_OTA_IMAGES][SHA_SIZE];
}sBootInfo_t;

void mcu_reset();
_i16 nwp_reset();


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
    unsigned long ulBootInfoToken;
    unsigned long ulBootInfoCreateFlag;

    //
    // Initialize boot info file create flag
    //
    ulBootInfoCreateFlag  = _FS_FILE_OPEN_FLAG_COMMIT|_FS_FILE_PUBLIC_WRITE;

    //
    // Check if its a secure MCU
    //
    if ( IsSecureMCU() )
    {
      ulBootInfoToken       = USER_BOOT_INFO_TOKEN;
      ulBootInfoCreateFlag  = _FS_FILE_OPEN_FLAG_COMMIT|_FS_FILE_OPEN_FLAG_SECURE|
                              _FS_FILE_OPEN_FLAG_NO_SIGNATURE_TEST|
                              _FS_FILE_PUBLIC_WRITE|_FS_FILE_OPEN_FLAG_VENDOR;
    }


	if (sl_FsOpen((unsigned char *)IMG_BOOT_INFO, FS_MODE_OPEN_WRITE, &ulBootInfoToken, &hndl)) {
		UARTprintf("error opening file, trying to create\n");

		if (sl_FsOpen((unsigned char *)IMG_BOOT_INFO, ulBootInfoCreateFlag, &ulBootInfoToken, &hndl)) {
			return -1;
		}
	}
	if( 0 < sl_FsWrite(hndl, 0, (_u8 *)psBootInfo, sizeof(sBootInfo_t)) )
	{
		UARTprintf("WriteBootInfo: ucActiveImg=%d, ulImgStatus=0x%x\n\r", psBootInfo->ucActiveImg, psBootInfo->ulImgStatus);
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
    if( 0 == sl_FsOpen((unsigned char *)IMG_BOOT_INFO, FS_MODE_OPEN_READ, &ulBootInfoToken, &lFileHandle) )
    {
        if( 0 < sl_FsRead(lFileHandle, 0, (_u8 *)psBootInfo, sizeof(sBootInfo_t)) )
        {
            status = 0;
            UARTprintf("ReadBootInfo: ucActiveImg=%d, ulImgStatus=0x%x\n\r", psBootInfo->ucActiveImg, psBootInfo->ulImgStatus);
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
    UARTprintf("_McuImageGetNewIndex: active image is %d, return new image %d \n\r", sBootInfo.ucActiveImg, newImageIndex);

    return newImageIndex;
}
#include "wifi_cmd.h"
#include "FreeRTOS.h"
#include "task.h"

void boot_commit_ota() {
    _ReadBootInfo(&sBootInfo);
    /* Check only on status TESTING */
    if( IMG_STATUS_TESTING == sBootInfo.ulImgStatus )
	{
		UARTprintf("Booted in testing mode\n");
		sBootInfo.ulImgStatus = IMG_STATUS_NOTEST;
		sBootInfo.ucActiveImg = (sBootInfo.ucActiveImg == IMG_ACT_USER1)?
								IMG_ACT_USER2:
								IMG_ACT_USER1;
		_WriteBootInfo(&sBootInfo);
		mcu_reset();
	}
}

#include "wifi_cmd.h"
int Cmd_version(int argc, char *argv[]) {
	UARTprintf( "ver: %d\nimg: %d\nstatus: %x\n", KIT_VER, sBootInfo.ucActiveImg, sBootInfo.ulImgStatus );
	return 0;
}

int wait_for_top_boot(unsigned int timeout);
int send_top(char *, int);
bool _decode_string_field(pb_istream_t *stream, const pb_field_t *field, void **arg);

#include "crypto.h"
#define SHA_SIZE 32
SHA1_CTX sha1ctx;

bool _on_file_download(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
	SyncResponse_FileDownload download_info;
	char * filename=NULL, * url=NULL, * host=NULL, * path=NULL, * serial_flash_path=NULL, * serial_flash_name=NULL;

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

	UARTprintf("ota - parsing\n" );
	if( !pb_decode(stream,SyncResponse_FileDownload_fields,&download_info) ) {
		UARTprintf("ota - parse fail \n" );
		return false;
	}
	filename = download_info.sd_card_filename.arg;
	path = download_info.sd_card_path.arg;
	url = download_info.url.arg;
	host = download_info.host.arg;
	serial_flash_name = download_info.serial_flash_filename.arg;
	serial_flash_path = download_info.serial_flash_path.arg;

	if( filename ) {
		UARTprintf( "ota - filename: %s\n", filename);
	}
	if( url ) {
		UARTprintf( "ota - url: %s\n",url);
	}
	if( host ) {
		UARTprintf( "ota - host: %s\n",host);
	}
	if( path ) {
		UARTprintf( "ota - path: %s\n",path);
	}
	if( serial_flash_path ) {
		UARTprintf( "ota - serial_flash_path: %s\n",serial_flash_path);
	}
	if( serial_flash_name ) {
		UARTprintf( "ota - serial_flash_name: %s\n",serial_flash_name);
	}
	if( download_info.has_copy_to_serial_flash ) {
		UARTprintf( "ota - copy_to_serial_flash: %s\n",download_info.copy_to_serial_flash);
	}

	if (filename && url && host && path) {
		int exists = 0;
		if (file_exists(filename, path)) {
			UARTprintf("ota - file exists, overwriting\n");
			exists = 1;
		}

		if(global_filename( filename ))
		{
			return 1;
		}
		if( exists ) {
			f_unlink(path_buff);
		}

		//download it!
		download_file( host, url, filename, path );

		if( download_info.has_copy_to_serial_flash && download_info.copy_to_serial_flash && serial_flash_name && serial_flash_path ) {

			char * full;
			char *buf;
			long sflash_fh = -1;
			WORD size=0;
			int status;
			full = pvPortMalloc(128);
			assert(full);
			buf = pvPortMalloc(512);
			assert(buf);
			memset(buf,0,sizeof(buf));
			memset(full,0,sizeof(full));

			strcpy(full, serial_flash_path);
			strcat(full, serial_flash_name);

			UARTprintf("copying %s %s\n", serial_flash_path, serial_flash_name);

			cd( path );
			if(global_filename( filename ))
			{
				return 1;
			}

			FRESULT res = f_open(&file_obj, path_buff, FA_READ);
			if( res != FR_OK ) {
				UARTprintf("ota - failed to open file %s", path_buff );
				return false;
			}

			f_stat( path_buff, &file_info );
			DWORD bytes_to_copy = file_info.fsize;

			if (strstr(full, "/sys/mcuimgx") != 0 )
			{
				_ReadBootInfo(&sBootInfo);
				full[11] = (_u8)_McuImageGetNewIndex() + '1'; /* mcuimg1 is for factory default, mcuimg2,3 are for OTA updates */
				UARTprintf("MCU image name converted to %s \n", full);
			}

			sl_FsOpen((unsigned char *)full, FS_MODE_OPEN_CREATE(bytes_to_copy, _FS_FILE_OPEN_FLAG_NO_SIGNATURE_TEST | _FS_FILE_OPEN_FLAG_COMMIT ), NULL, &sflash_fh);
			if( res != FR_OK ) {
				UARTprintf("ota - failed to open file %s\n", full );
				return false;
			}
			int file_len = bytes_to_copy;

			play_led_progress_bar(254, 132, 4, 0);

			if( download_info.has_sha1 ) {
				SHA1_Init(&sha1ctx);
			}

			UARTprintf( "copying %d from %s on sd to %s on sflash\n", bytes_to_copy, path_buff, full);
			while( bytes_to_copy > 0 ) {
				//read from sd into buff
				res = f_read( &file_obj, buf, bytes_to_copy<512?bytes_to_copy:512, &size );
				if( res != FR_OK ) {
					UARTprintf("ota - failed to read file %s\n", path_buff );
					return false;
				}
				//UARTprintf( "offset %d, left %d, chunk %d\n",  file_len-bytes_to_copy, bytes_to_copy,size );
				set_led_progress_bar( 100*(file_len-bytes_to_copy)/file_len );

				if( download_info.has_sha1 ) {
					SHA1_Update(&sha1ctx, (uint8_t*)buf, size);
				}

				status = sl_FsWrite(sflash_fh, file_len-bytes_to_copy, (unsigned char*)buf, size);
				if( status != size ) {
					UARTprintf("ota - failed to write file %s\n", full );
					return false;
				}
				bytes_to_copy -= size;
			}
			UARTprintf( "done, closing\n" );
			sl_FsClose(sflash_fh,0,0,0);
			f_close(&file_obj);

			if( strcmp(full, "/top/update") == 0 ) {
				send_top("dfu", strlen("dfu"));
				wait_for_top_boot(120000);
			}
		}
		if( download_info.has_reset_application_processor && download_info.reset_application_processor ) {
			UARTprintf("change image status to IMG_STATUS_TESTREADY\n\r");
			_ReadBootInfo(&sBootInfo);
			if (download_info.has_sha1) {
				unsigned char sha[SHA_SIZE] = {0};

				SHA1_Final(sha, &sha1ctx);

				if (memcmp(sha, download_info.sha1.bytes, SHA_SIZE) == 0) {
					sBootInfo.ulImgStatus = IMG_STATUS_TESTREADY;
					memcpy(sBootInfo.sha[_McuImageGetNewIndex()], download_info.sha1.bytes, SHA_SIZE );
				} else {
					UARTprintf( "fw update SHA did not match!\n");
				}
			} else {
				sBootInfo.ulImgStatus = IMG_STATUS_TESTREADY;
				UARTprintf( "warning no download SHA on fw!\n");
			}
			//sBootInfo.ucActiveImg this is set by boot loader
			_WriteBootInfo(&sBootInfo);
			mcu_reset();
		}
		if( download_info.has_reset_network_processor && download_info.reset_network_processor ) {
			UARTprintf( "reset nwp\n" );
			nwp_reset();
		}
	}

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

	return true;
}
