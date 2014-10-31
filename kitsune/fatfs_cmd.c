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

#include "stdlib.h"
#include "uart.h"
#include "hw_memmap.h"
void UARTStdioIntHandler(void);

int
Cmd_write_file(int argc, char *argv[])
{
	UARTprintf("Cmd_write_file\n");

	WORD bytes = 0;
	WORD bytes_written = 0;
	WORD bytes_to_write = atoi(argv[2]);

    if(global_filename( argv[1] ))
    {
    	return 1;

    }

    // Open the file for writing.
    FRESULT res = f_open(&file_obj, path_buff, FA_CREATE_NEW|FA_WRITE);
    UARTprintf("res :%d\n",res);

    if(res != FR_OK && res != FR_EXIST){
    	UARTprintf("File open %s failed: %d\n", path_buff, res);
    	return res;
    }

	UARTIntUnregister(UARTA0_BASE); //Ahoy matey, I be takin yer uart
    do {
		uint8_t c = UARTCharGet(UARTA0_BASE);
		res = f_write( &file_obj, (void*)&c, 1, &bytes );
		bytes_written+=bytes;
		UARTCharPutNonBlocking(UARTA0_BASE, 52u); //basic feedback
    } while( bytes_written < bytes_to_write );

    res = f_close( &file_obj );

	UARTIntRegister(UARTA0_BASE, UARTStdioIntHandler);
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

    SockID = socket(SL_AF_INET,SL_SOCK_STREAM, 0);
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
int GetData(char * filename, char* url, char * host)
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
    transfer_len -= (pBuff - g_buff);

    // If data in chunked format, calculate the chunk size
    if(isChunked == 1)
    {
        r = GetChunkSize(&transfer_len, &pBuff, &recv_size);
    }

    /* Open file to save the downloaded file */
    if(global_filename( filename ))
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
    uint32_t total = recv_size;
    int percent = 101-100*recv_size/total;
    while (0 < transfer_len)
    {
    	if( 101-100*recv_size/total != percent ) {
    		percent = 101-100*recv_size/total;
    		UARTprintf("dl loop %d %d\r\n", recv_size, percent );
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
                    return((int)res);
                }
                f_unlink( path_buff );
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
                            return((int)res);
                        }
                        f_unlink( path_buff );

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
                            return((int)res);
                        }
                        f_unlink( path_buff );
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
                    return((int)res);
                }
                f_unlink( path_buff );
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
            return TCP_RECV_ERROR;
        }

        pBuff = g_buff;
    }

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
            return((int)res);
        }
        f_unlink( path_buff );
        return INVALID_FILE;
    }
    else
    {
    	UARTprintf(" successful file\r\n" );
        /* Save and close file */
        res = f_close( &file_obj );

        if(res != FR_OK)
        {
            return((int)res);
        }
    }

    return 0;
}

int Cmd_download(int argc, char*argv[]) {
	unsigned long ip;
	char host[128];
	char filename[128];
	char url[256];

	strncpy( host, argv[1], 128 );
	strncpy( filename, argv[2], 128 );
	strncpy( url, argv[3], 256 );

    int r = gethostbyname(host, strlen(host), &ip,SL_AF_INET);
    if(r < 0)
    {
        ASSERT_ON_ERROR(GET_HOST_IP_FAILED);
    }
	UARTprintf("download <host> <filename> <url>\n\r");
    // Create a TCP connection to the Web Server
    dl_sock = CreateConnection(ip);

    if(dl_sock < 0)
    {
        UARTprintf("Connection to server failed\n\r");
        return SERVER_CONNECTION_FAILED;
    }
    else
    {
        UARTprintf("Connection to server created successfully\r\n");
    }
    // Download the file, verify the file and replace the exiting file
    r = GetData(filename, url, host);
    if(r < 0)
    {
        UARTprintf("Device couldn't download the file from the server\n\r");
    }

    r = close(dl_sock);

    return r;
}

//end download functions

