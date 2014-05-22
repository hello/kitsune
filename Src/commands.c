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


/* FatFS include */
#include "fatfs/src/ff.h"
#include "fatfs/src/diskio.h"

/* protobuf includes */
#include <pb_encode.h>
#include <pb_decode.h>
#include "simple.pb.h"

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* Standard Stellaris includes */
#include "inc/hw_memmap.h"
#include "inc/hw_sysctl.h"
#include "inc/hw_types.h"

#include "driverlib/debug.h"
#include "driverlib/gpio.h"
#include "driverlib/uart.h"

/* Other Stellaris include */
#include "grlib/grlib.h"
#include "drivers/cfal96x64x16.h"
#include "utils/cpu_usage.h"
#include "utils/cmdline.h"
#include "utils/ustdlib.h"

#include "uartstdio.h"

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
static char g_pcCwdBuf[PATH_BUF_SIZE] = "/";

//*****************************************************************************
//
// A temporary data buffer used when manipulating file paths, or reading data
// from the SD card.
//
//*****************************************************************************
static char g_pcTmpBuf[PATH_BUF_SIZE];

// ==============================================================================
// The CPU usage in percent, in 16.16 fixed point format.
// ==============================================================================
extern unsigned long g_ulCPUUsage;

typedef struct taskParams
{
	xSemaphoreHandle	blinkSema;
	xSemaphoreHandle	displaySema;
	signed short		color;
} taskParams_t;

extern taskParams_t* g_pTaskParams;

//*****************************************************************************
//
// The following are data structures used by FatFs.
//
//*****************************************************************************
static FATFS g_sFatFs;
static DIR g_sDirObject;
static FILINFO file_info;
static FIL file_obj;

// ==============================================================================
// This function implements the "cpu" command.  It prints the CPU type, speed
// and current percentage utilization.
// ==============================================================================
int Cmd_cpu(int argc, char *argv[])
{
    //
    // Print some header text.
    //
    UARTprintf("ARM Cortex-M4F %u MHz - ",SysCtlClockGet() / 1000000);
    UARTprintf("%2u%% utilization\n", (g_ulCPUUsage+32768) >> 16);

    // Return success.
    return(0);
}


// ==============================================================================
// This function implements the "free" command.  It prints the free memory.
// ==============================================================================
int Cmd_free(int argc, char *argv[])
{
    //
    // Print some header text.
    //
    UARTprintf("%d bytes free\n", xPortGetFreeHeapSize());

    // Return success.
    return(0);
}

// ==============================================================================
// This function implements the "tasks" command.  It prints a list of the
// current FreeRTOS tasks with information about status, stack usage, etc.
// ==============================================================================
#if ( configUSE_TRACE_FACILITY == 1 )

extern char *__stack;
int Cmd_tasks(int argc, char *argv[])
{
	signed char*	pBuffer;
	unsigned char*	pStack;
	portBASE_TYPE	x;
	
	pBuffer = pvPortMalloc(1024);
	vTaskList(pBuffer);
	UARTprintf("\t\t\t\t\tUnused\nTaskName\t\tStatus\tPri\tStack\tTask ID\n");
	UARTprintf("=======================================================");
	UARTprintf("%s", pBuffer);
	
	// Calculate kernel stack usage
	x = 0;
	pStack = (unsigned char *) &__stack;
	while (*pStack++ == 0xA5)
	{
		x++;
	}
	usprintf((char *) pBuffer, "%%%us", configMAX_TASK_NAME_LEN);
	usprintf((char *) &pBuffer[10], (const char *) pBuffer, "kernel");
	UARTprintf("%s\t\tR\t%u\t%u\t-\n", &pBuffer[10], configKERNEL_INTERRUPT_PRIORITY,
		x / sizeof(portBASE_TYPE));
	vPortFree(pBuffer);
	return 0;
}

#endif /* configUSE_TRACE_FACILITY */

// ==============================================================================
// This function implements the "help" command.  It prints a simple list of the
// available commands with a brief description.
// ==============================================================================
int Cmd_help(int argc, char *argv[])
{
    tCmdLineEntry *pEntry;

    //
    // Print some header text.
    //
    UARTprintf("\nAvailable commands\n");
    UARTprintf("------------------\n");

    //
    // Point at the beginning of the command table.
    //
    pEntry = &g_sCmdTable[0];

    //
    // Enter a loop to read each entry from the command table.  The end of the
    // table has been reached when the command name is NULL.
    //
    while(pEntry->pcCmd)
    {
        //
        // Print the command name and the brief description.
        //
        UARTprintf("%s: %s\n", pEntry->pcCmd, pEntry->pcHelp);

        //
        // Advance to the next entry in the table.
        //
        pEntry++;
    }

    //
    // Return success.
    //
    return(0);
}

int Cmd_protobuftest(int argc, char *argv[])
{
    /* This is the buffer where we will store our message. */
    uint8_t buffer[128];
    size_t message_length;
    bool status;

    /* Encode our message */
    {
        /* Allocate space on the stack to store the message data.
         *
         * Nanopb generates simple struct definitions for all the messages.
         * - check out the contents of simple.pb.h! */
        SimpleMessage message;

        /* Create a stream that will write to our buffer. */
        pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

        /* Fill in the lucky number */
        message.lucky_number = 13;

        /* Now we are ready to encode the message! */
        status = pb_encode(&stream, SimpleMessage_fields, &message);
        message_length = stream.bytes_written;

        /* Then just check for any errors.. */
        if (!status)
        {
        	UARTprintf("Encoding failed: %s\n", PB_GET_ERROR(&stream));
            return 1;
        }
    }

    /* Now we could transmit the message over network, store it in a file or
     * wrap it to a pigeon's leg.
     */

    /* But because we are lazy, we will just decode it immediately. */

    {
        /* Allocate space for the decoded message. */
        SimpleMessage message;

        /* Create a stream that reads from the buffer. */
        pb_istream_t stream = pb_istream_from_buffer(buffer, message_length);

        /* Now we are ready to decode the message. */
        status = pb_decode(&stream, SimpleMessage_fields, &message);

        /* Check for errors... */
        if (!status)
        {
        	UARTprintf("Decoding failed: %s\n", PB_GET_ERROR(&stream));
            return 1;
        }

        /* Print the data contained in the message. */
        UARTprintf("lucky number %d!\n", message.lucky_number);
    }

    return 0;
}
int Cmd_mnt(int argc, char *argv[])
{
    FRESULT res;
	//
	// Print hello message to user.
	//
	UARTprintf("\n\nSD Card Mounting...\n");

	//
	// Mount the file system, using logical disk 0.
	//
	res = f_mount(0, &g_sFatFs);
	if(res != FR_OK)
	{
		UARTprintf("f_mount error: %i\n", (res));
		return(1);
	}
	UARTprintf("f_mount success\n");
	return 0;
}
int Cmd_umnt(int argc, char *argv[])
{
    FRESULT res;
	//
	// Print hello message to user.
	//
	UARTprintf("\n\nSD Card Unmounting...\n");

	//
	// Mount the file system, using logical disk 0.
	//
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
	//
	// Print hello message to user.
	//
	UARTprintf("\n\nMaking FS...\n");

	//
	// Mount the file system, using logical disk 0.
	//
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

    //
    // Open the current directory for access.
    //
    res = f_opendir(&g_sDirObject, g_pcCwdBuf);

    //
    // Check for error and return if there is a problem.
    //
    if(res != FR_OK)
    {
        return((int)res);
    }

    ui32TotalSize = 0;
    ui32FileCount = 0;
    ui32DirCount = 0;

    //
    // Give an extra blank line before the listing.
    //
    UARTprintf("\n");

    //
    // Enter loop to enumerate through all directory entries.
    //
    for(;;)
    {
        //
        // Read an entry from the directory.
        //
        res = f_readdir(&g_sDirObject, &file_info);

        //
        // Check for error and return if there is a problem.
        //
        if(res != FR_OK)
        {
            return((int)res);
        }

        //
        // If the file name is blank, then this is the end of the listing.
        //
        if(!file_info.fname[0])
        {
            break;
        }

        //
        // If the attribue is directory, then increment the directory count.
        //
        if(file_info.fattrib & AM_DIR)
        {
            ui32DirCount++;
        }

        //
        // Otherwise, it is a file.  Increment the file count, and add in the
        // file size to the total.
        //
        else
        {
            ui32FileCount++;
            ui32TotalSize += file_info.fsize;
        }

        //
        // Print the entry information on a single line with formatting to show
        // the attributes, date, time, size, and name.
        //
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

    //
    // Print summary lines showing the file, dir, and size totals.
    //
    UARTprintf("\n%4u File(s),%10u bytes total\n%4u Dir(s)",
                ui32FileCount, ui32TotalSize, ui32DirCount);

    //
    // Get the free space.
    //
    res = f_getfree("/", (DWORD *)&ui32TotalSize, &psFatFs);

    //
    // Check for error and return if there is a problem.
    //
    if(res != FR_OK)
    {
        return((int)res);
    }

    //
    // Display the amount of free space that was calculated.
    //
    UARTprintf(", %10uK bytes free\n", (ui32TotalSize *
                                        psFatFs->sects_clust / 2));

    //
    // Made it to here, return with no errors.
    //
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

    //
    // Copy the current working path into a temporary buffer so it can be
    // manipulated.
    //
    strcpy(g_pcTmpBuf, g_pcCwdBuf);

    //
    // If the first character is /, then this is a fully specified path, and it
    // should just be used as-is.
    //
    if(argv[1][0] == '/')
    {
        //
        // Make sure the new path is not bigger than the cwd buffer.
        //
        if(strlen(argv[1]) + 1 > sizeof(g_pcCwdBuf))
        {
            UARTprintf("Resulting path name is too long\n");
            return(0);
        }

        //
        // If the new path name (in argv[1])  is not too long, then copy it
        // into the temporary buffer so it can be checked.
        //
        else
        {
            strncpy(g_pcTmpBuf, argv[1], sizeof(g_pcTmpBuf));
        }
    }

    //
    // If the argument is .. then attempt to remove the lowest level on the
    // CWD.
    //
    else if(!strcmp(argv[1], ".."))
    {
        //
        // Get the index to the last character in the current path.
        //
        ui8Idx = strlen(g_pcTmpBuf) - 1;

        //
        // Back up from the end of the path name until a separator (/) is
        // found, or until we bump up to the start of the path.
        //
        while((g_pcTmpBuf[ui8Idx] != '/') && (ui8Idx > 1))
        {
            //
            // Back up one character.
            //
            ui8Idx--;
        }

        //
        // Now we are either at the lowest level separator in the current path,
        // or at the beginning of the string (root).  So set the new end of
        // string here, effectively removing that last part of the path.
        //
        g_pcTmpBuf[ui8Idx] = 0;
    }

    //
    // Otherwise this is just a normal path name from the current directory,
    // and it needs to be appended to the current path.
    //
    else
    {
        //
        // Test to make sure that when the new additional path is added on to
        // the current path, there is room in the buffer for the full new path.
        // It needs to include a new separator, and a trailing null character.
        //
        if(strlen(g_pcTmpBuf) + strlen(argv[1]) + 1 + 1 > sizeof(g_pcCwdBuf))
        {
            UARTprintf("Resulting path name is too long\n");
            return(0);
        }

        //
        // The new path is okay, so add the separator and then append the new
        // directory to the path.
        //
        else
        {
            //
            // If not already at the root level, then append a /
            //
            if(strcmp(g_pcTmpBuf, "/"))
            {
                strcat(g_pcTmpBuf, "/");
            }

            //
            // Append the new directory to the path.
            //
            strcat(g_pcTmpBuf, argv[1]);
        }
    }

    //
    // At this point, a candidate new directory path is in chTmpBuf.  Try to
    // open it to make sure it is valid.
    //
    res = f_opendir(&g_sDirObject, g_pcTmpBuf);

    //
    // If it can't be opened, then it is a bad path.  Inform the user and
    // return.
    //
    if(res != FR_OK)
    {
        UARTprintf("cd: %s\n", g_pcTmpBuf);
        return((int)res);
    }

    //
    // Otherwise, it is a valid new path, so copy it into the CWD.
    //
    else
    {
        strncpy(g_pcCwdBuf, g_pcTmpBuf, sizeof(g_pcCwdBuf));
    }

    //
    // Return success.
    //
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
    //
    // Print the CWD to the console.
    //
    UARTprintf("%s\n", g_pcCwdBuf);

    //
    // Return success.
    //
    return(0);
}
int global_filename(char * local_fn)
{
    //
    // First, check to make sure that the current path (CWD), plus the file
    // name, plus a separator and trailing null, will all fit in the temporary
    // buffer that will be used to hold the file name.  The file name must be
    // fully specified, with path, to FatFs.
    //
    if(strlen(g_pcCwdBuf) + strlen(local_fn) + 1 + 1 > sizeof(g_pcTmpBuf))
    {
        UARTprintf("Resulting path name is too long\n");
        return(0);
    }

    //
    // Copy the current path to the temporary buffer so it can be manipulated.
    //
    strcpy(g_pcTmpBuf, g_pcCwdBuf);

    //
    // If not already at the root level, then append a separator.
    //
    if(strcmp("/", g_pcCwdBuf))
    {
        strcat(g_pcTmpBuf, "/");
    }

    //
    // Now finally, append the file name to result in a fully specified file.
    //
    strcat(g_pcTmpBuf, local_fn);

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

    //
    // Open the file for reading.
    //
    res = f_open(&file_obj, g_pcTmpBuf, FA_READ);

    //
    // If there was some problem opening the file, then return an error.
    //
    if(res != FR_OK)
    {
        return((int)res);
    }

    //
    // Enter a loop to repeatedly read data from the file and display it, until
    // the end of the file is reached.
    //
    do
    {
        //
        // Read a block of data from the file.  Read as much as can fit in the
        // temporary buffer, including a space for the trailing null.
        //
        res = f_read(&file_obj, g_pcTmpBuf, 4,
                         &ui16BytesRead);

        //
        // If there was an error reading, then print a newline and return the
        // error to the user.
        //
        if(res != FR_OK)
        {
            UARTprintf("\n");
            return((int)res);
        }

        //
        // Null terminate the last block that was read to make it a null
        // terminated string that can be used with printf.
        //
        g_pcTmpBuf[ui16BytesRead] = 0;

        //
        // Print the last chunk of the file that was received.
        //
        UARTprintf("%s", g_pcTmpBuf);
    }
    while(ui16BytesRead == 4);

    f_close( &file_obj );

    UARTprintf("\n");
    //
    // Return success.
    //
    return(0);
}
int
Cmd_write(int argc, char *argv[])
{
    FRESULT res;

	WORD bytes = 0;
	WORD bytes_written = 0;
	WORD bytes_to_write = strlen(argv[2])+1;

    if(global_filename( argv[1] ))
    {
    	return 1;
    }

    //
    // Open the file for reading.
    //
    res = f_open(&file_obj, g_pcTmpBuf, FA_CREATE_NEW|FA_WRITE);

    f_stat( g_pcTmpBuf, &file_info );

    if( file_info.fsize != 0 )
        res = f_lseek(&file_obj, file_info.fsize );

    do {
		res = f_write( &file_obj, argv[2]+bytes_written, bytes_to_write-bytes_written, &bytes );
		bytes_written+=bytes;
    } while( bytes_written < bytes_to_write );

    res = f_close( &file_obj );

    //
    // If there was some problem opening the file, then return an error.
    //
    if(res != FR_OK)
    {
        return((int)res);
    }

    //
    // Return success.
    //
    return(0);
}

int
Cmd_rm(int argc, char *argv[])
{
    FRESULT res;

    if(global_filename( argv[1] ))
    {
    	return 1;
    }

    //
    // Remove file.
    //
    res = f_unlink( g_pcTmpBuf);

    //
    // If there was some problem opening the file, then return an error.
    //
    if(res != FR_OK)
    {
        return((int)res);
    }

    //
    // Return success.
    //
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

    res = f_mkdir( g_pcTmpBuf);
    if(res != FR_OK)
    {
        return((int)res);
    }
    return(0);
}

// ==============================================================================
// This is the table that holds the command names, implementing functions, and
// brief description.
// ==============================================================================
tCmdLineEntry g_sCmdTable[] =
{
    { "help",     Cmd_help,     "Display list of commands" },
    { "?",        Cmd_help,     "alias for help" },
    { "cpu",      Cmd_cpu,      "Show CPU utilization" },
    { "free",     Cmd_free,     "Report free memory" },
    { "mnt",      Cmd_mnt,      "Mount the SD card" },
    { "umnt",     Cmd_umnt,     "Unount the SD card" },
    { "ls",       Cmd_ls,       "Display list of files" },
    { "chdir",    Cmd_cd,       "Change directory" },
    { "cd",       Cmd_cd,       "alias for chdir" },
    { "mkdir",    Cmd_mkdir,    "make a directory" },
    { "rm",       Cmd_rm,       "Remove file" },
    { "write",    Cmd_write,    "Write some text to a file" },
    { "mkfs",     Cmd_mkfs,     "Make filesystem" },
    { "pwd",      Cmd_pwd,      "Show current working directory" },
    { "cat",      Cmd_cat,      "Show contents of a text file" },

#if ( configUSE_TRACE_FACILITY == 1 )
	{ "tasks",    Cmd_tasks,     "Report stats of all tasks" },
#endif
	{ "pb",    Cmd_protobuftest,     "Test simple protobuf" },

    { 0, 0, 0 }
};

// ==============================================================================
// This is the UARTTask.  It handles command lines received from the RX IRQ.
// ==============================================================================
extern xSemaphoreHandle g_xRxLineSemaphore;
void UARTStdioIntHandler(void);

void vUARTTask( void *pvParameters )
{
    char    cCmdBuf[64];
    int     nStatus;
	
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_GPIOA);

    //
    // Configure the pin muxing for UART0 functions on port A0 and A1.
    // This step is not necessary if your part does not support pin muxing.
    // TODO: change this to select the port/pin you are using.
    //
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);

    //
    // Select the alternate (UART) function for these pins.
    // TODO: change this to select the port/pin you are using.
    //
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);

    //
    // Initialize the UART for console I/O.
    //
    UARTStdioInit(0);

    SysCtlPeripheralSleepEnable(SYSCTL_PERIPH_UART0);

    UARTIntRegister(UART0_BASE, UARTStdioIntHandler);

	UARTprintf("\n\nFreeRTOS %s Demo for EK-LM4F232 Eval Board\n",
		tskKERNEL_VERSION_NUMBER);
	UARTprintf("\n? for help\n");
	UARTprintf("> ");    

	/* Loop forever */
    while (1)
    {
		/* Wait for a signal indicating we have an RX line to process */
 		xSemaphoreTake(g_xRxLineSemaphore, portMAX_DELAY);
    	
	    if (UARTPeek('\r') != -1)
	    {
	    	/* Read data from the UART and process the command line */
	        UARTgets(cCmdBuf, sizeof(cCmdBuf));
	        if (ustrlen(cCmdBuf) == 0)
	        {
	            UARTprintf("> ");
	            continue;
	        }
	
	        //
	        // Pass the line from the user to the command processor.  It will be
	        // parsed and valid commands executed.
	        //
	        nStatus = CmdLineProcess(cCmdBuf);
	
	        //
	        // Handle the case of bad command.
	        //
	        if(nStatus == CMDLINE_BAD_CMD)
	        {
	            UARTprintf("Bad command!\n");
	        }
	
	        //
	        // Handle the case of too many arguments.
	        //
	        else if(nStatus == CMDLINE_TOO_MANY_ARGS)
	        {
	            UARTprintf("Too many arguments for command processor!\n");
	        }
	
	        //
	        // Otherwise the command was executed.  Print the error code if one was
	        // returned.
	        //
	        else if(nStatus != 0)
	        {
	            UARTprintf("Command returned error code %d\n",nStatus);
	        }
	
	        UARTprintf("> ");
	    }
    }
}
