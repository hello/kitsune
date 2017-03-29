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
#include "lfsr/pn_stream.h"

#include "FreeRTOS.h"

#include "kitsune_version.h"

//begin download stuff
#include "wlan.h"
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
int sf_sha1_verify(const char * sha_truth, const char * serial_file_path);

#define HLO_HTTP_ERR -1000
#define HTTP_RETRIES 5

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
 FATFS fsobj;
 DIR fsdirobj;
 FILINFO file_info;
 FIL file_obj;


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
// An optional string as paramter can be used after special cases such as
// Audio = $a46000
//
//*****************************************************************************

char* strsep( char** stringp, const char* delim )
  {
  char* result;

  if ((stringp == NULL) || (*stringp == NULL)) return NULL;

  result = *stringp;

  while (**stringp && !strchr( delim, **stringp )) ++*stringp;

  if (**stringp) *(*stringp)++ = '\0';
  else             *stringp    = NULL;

  return result;
  }


extern volatile int sys_volume;
#include "hlo_pipe.h"
#include "hlo_audio.h"
#include "fs_utils.h"
#include "hlo_http.h"
#include "audio_types.h"
#include "hlo_audio_tools.h"
int32_t set_volume(int v, unsigned int dly);
#define BUF_SIZE 64
hlo_stream_t * open_stream_from_path(char * str, uint8_t input){
	hlo_stream_t * rstr = NULL;
	char * s = str+1;
	char * p;
	if(input){//input
		while(p = strsep (&s,"$")) {
			switch(p[0]){
				case 'a':
				case 'A':
				{
					int opt_rate = 0;
					if(p[1] != '\0'){
						opt_rate = ustrtoul(p+1,NULL, 10);
					}
					DISP("Input Opt rate is %d\r\n", opt_rate);
					if(opt_rate){
						rstr = hlo_audio_open_mono(opt_rate,HLO_AUDIO_RECORD);
					}else{
						rstr = hlo_audio_open_mono(AUDIO_SAMPLE_RATE,HLO_AUDIO_RECORD);
					}
				}
				break;
				case 'b':
				case 'B':
					rstr = hlo_stream_bw_limited(rstr, 32, 5000);
					break;
#if 0
				case 'n':
				case 'N':
					rstr = hlo_stream_nn_keyword_recognition( rstr, 80 );
					break;
#endif
				case 't':
				case 'T':
					rstr = hlo_stream_en( rstr );
					break;
				case 'c':
					rstr = hlo_stream_sr_cnv( rstr, DOWNSAMPLE );
					break;
				case 'C':
					rstr = hlo_stream_sr_cnv( rstr, UPSAMPLE );
					break;
				case 'l':
				case 'L':
					rstr = hlo_light_stream( rstr , true);
					break;
				case 'r':
				case 'R':
					rstr = random_stream_open();
					break;
				case 'i':
				case 'I':
					rstr =  hlo_http_get(p+1);
					break;
				case 'f':
				case 'F':
					global_filename(p+1);
					if(input > 1){//repeating mode
						rstr =  fs_stream_open_media(path_buff, -1);
					} else {
						rstr = fs_stream_open(path_buff, HLO_STREAM_READ);
					}
					break;
				case '0':
					rstr = zero_stream_open();
					break;
				case '~':
					rstr = open_serial_flash(p+1, HLO_STREAM_READ,  1024*128);
					break;
				default:
					LOGE("stream missing\n");
					break;
			}
		}
	}else{//output
		while(p = strsep (&s,"$")) {
			switch(p[0]){
				case 'a':
				case 'A':
				{
					int opt_rate = 0;
					if(str[2] != '\0'){
						opt_rate = ustrtoul(p+1,NULL, 10);
					}
					DISP("Output Opt rate is %d\r\n", opt_rate);
					if(opt_rate){
						rstr = hlo_audio_open_mono(opt_rate,HLO_AUDIO_PLAYBACK);
					}else{
						rstr = hlo_audio_open_mono(AUDIO_SAMPLE_RATE,HLO_AUDIO_PLAYBACK);
					}
					set_volume(sys_volume, portMAX_DELAY);
				}
				break;
				case 'b':
				case 'B':
					rstr = hlo_stream_bw_limited(rstr, 32, 5000);
					break;
				case 'i':
				case 'I':
					rstr = hlo_http_post(p+1, NULL);
					break;
				case 'o':
				case 'O':
					rstr = uart_stream();
					break;
				case '~':
					rstr = open_serial_flash(p+1, HLO_STREAM_WRITE, 1024*128);
					break;
				case 'f':
				case 'F':
					global_filename(p+1);
					rstr = fs_stream_open(path_buff, HLO_STREAM_WRITE);
					break;
				default:
					rstr = random_stream_open();
				}
			}
		}
	return rstr;
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

//end download functions
#include "sync_response.pb.h"
#include "stdbool.h"
#include "pb.h"
#include "pb_decode.h"
#include "crypto.h"

/******************************************************************************
   Image file names
*******************************************************************************/
#define IMG_BOOT_INFO           "/ota/mcubootinfo.bin"
#define IMG_FACTORY_DEFAULT     "/ota/mcuimg1.bin"
#define IMG_USER_1              "/ota/mcuimg2.bin"
#define IMG_USER_2              "/ota/mcuimg3.bin"

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


/* Save bootinfo on ImageCommit call */
static sBootInfo_t sBootInfo;

static _i32 _WriteBootInfo(sBootInfo_t *psBootInfo)
{
    _i32 status = -1;
    _i32 hndl;
    unsigned long ulBootInfoToken = 0;
    unsigned long ulBootInfoCreateFlag;

    //
    // Initialize boot info file create flag
    //
    ulBootInfoCreateFlag  = SL_FS_CREATE | SL_FS_OVERWRITE | SL_FS_CREATE_NOSIGNATURE | SL_FS_CREATE_MAX_SIZE( 256 );
    hndl = sl_FsOpen((unsigned char *)IMG_BOOT_INFO, ulBootInfoCreateFlag, &ulBootInfoToken);
	if (hndl < 0) {
		LOGE("Boot info open failed\n");
		return -1;
	}
	status = sl_FsWrite(hndl, 0, (_u8 *)psBootInfo, sizeof(sBootInfo_t));
	if( status >= 0 )
	{
		LOGI("WriteBootInfo: ucActiveImg=%d, ulImgStatus=0x%x\n\r", psBootInfo->ucActiveImg, psBootInfo->ulImgStatus);
	} else {
		LOGE("FAILED TO WRITE BOOT INFO %d\n", status);
	}
	sl_FsClose(hndl, 0, 0, 0);
    return 0;
}

static _i32 _ReadBootInfo(sBootInfo_t *psBootInfo)
{
    _i32 hndl;
    _i32 status = -1;
    unsigned long ulBootInfoToken;
    //
    // Check if its a secure MCU
    //
    hndl = sl_FsOpen((unsigned char *)IMG_BOOT_INFO, SL_FS_READ, &ulBootInfoToken);
    if( hndl >= 0 )
    {
    	status =  sl_FsRead(hndl, 0, (_u8 *)psBootInfo, sizeof(sBootInfo_t));
        if( status >= 0 )
        {
            static bool printed = false;
            if( !printed ) {
            	LOGI("ReadBootInfo: ucActiveImg=%d, ulImgStatus=0x%x\n\r", psBootInfo->ucActiveImg, psBootInfo->ulImgStatus);
            	printed = true;
            }
        } else {
    		LOGE("FAILED TO READ BOOT INFO %d\n", status);
        }
        sl_FsClose(hndl, 0, 0, 0);
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
        if( 0 == verify_top_update() ){
            LOGI("Updating top board\r\n");
            activate_top_ota();
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

hlo_stream_t * hlo_http_get_opt(hlo_stream_t * sock, const char * host, const char * endpoint);

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
            if(global_filename( filename ))
            {
                goto end_download_task;
            }
            hello_fs_unlink(path_buff);
            DISP("Deleted file: %s\n", path_buff);

            char path_buf[64] = {0};

			hlo_stream_t * sf_str, *http_str, *sock_str;
			sock_str = hlo_sock_stream(host, false);
			http_str = hlo_http_get_opt( sock_str, host, url );

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

				strncpy(path_buf, serial_flash_path, 32 );
				strncat(path_buf, serial_flash_name, 32 );

				if(strstr(path_buf, "top/top.bin")){
					memset(path_buf, 0, sizeof(path_buf));
					strcpy(path_buf,"/top/update.bin");
					LOGW("Wrong top board name used, updating path to %s\r\n", path_buf);
				}

				//TODO get max size from protobuf and set it here
				sf_str = open_serial_flash(path_buf, HLO_STREAM_WRITE, download_info.has_file_size ? download_info.file_size : 300 * 1024);
				if(sf_str){
					hlo_filter_data_transfer(http_str, sf_str, NULL, NULL);
					DISP("filesize %d, transferred %d\r\n", download_info.file_size, sf_str->info.bytes_written);
					hlo_stream_close(sf_str);
				}

                if (strcmp(path_buf, "/top/update.bin") == 0) {
                    if (download_info.has_sha1) {
                        memcpy(top_sha_cache, download_info.sha1.bytes, SHA1_SIZE );
                        if( sf_sha1_verify((char *)download_info.sha1.bytes, path_buf)){
                            LOGW("Top DFU download failed\r\n");
                            top_need_dfu = 0;
                            goto end_download_task;
                        }else{
                            top_need_dfu = 1;
                        }
                    }else{
                    	LOGE("NO sha\r\n");
                    }
                }
                LOGI("done, closing\n");
			} else {
				char buf[512];
				// Set file download pending for download manager
				update_file_download_status(true);

				// Delete corresponding SHA file if exists
				// delete before downloading the file, so that if the download fails midway,
				// the device won't be left with a corrupt file and a valid sha file.
				update_sha_file(path, filename, sha_file_delete,NULL, false );

				strncpy( buf, path, 64 );
				strncat(buf, "/", 64 );
				strncat(buf, filename, 64 );

				sf_str = fs_stream_open(buf, HLO_STREAM_CREATE_NEW);

				while(1){
					if(hlo_stream_transfer_between( http_str, sf_str, (uint8_t*)buf, sizeof(buf), 4 ) < 0){
						break;
					}
					DISP("x");
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
    unsigned char sha[SHA1_SIZE] = { 0 };
    static unsigned char buffer[128];
    SHA1_CTX sha1ctx;
    LOGI( "computing SHA of %s\n", serial_file_path);
    hlo_stream_t * fs = open_serial_flash((char*)serial_file_path, HLO_STREAM_READ, 0);
    SHA1_Init(&sha1ctx);
    if(fs){
    	while(1){
    		int read = hlo_stream_transfer_all(FROM_STREAM, fs, buffer,sizeof(buffer),4);
    		if(read > 0){
    			SHA1_Update(&sha1ctx, buffer, read);
    		}else{
    			break;
    		}
    	}
    	hlo_stream_close(fs);
    	SHA1_Final(sha, &sha1ctx);
    }else{
    	LOGI("%s does not exist/error!\r\n", serial_file_path);
    	return -1;
    }
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

