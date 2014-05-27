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

#include <hw_types.h>
#include <hw_memmap.h>
#include <prcm.h>
#include <pin.h>
#include <uart.h>
#include <stdarg.h>
#include <stdlib.h>
#include "rom_map.h"

/* protobuf includes */
#include <pb_encode.h>
#include <pb_decode.h>
#include "simple.pb.h"

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "fault.h"

#include "cmdline.h"
#include "ustdlib.h"
#include "uartstdio.h"

//// ==============================================================================
//// The CPU usage in percent, in 16.16 fixed point format.
//// ==============================================================================
//extern unsigned long g_ulCPUUsage;
//
//// ==============================================================================
//// This function implements the "cpu" command.  It prints the CPU type, speed
//// and current percentage utilization.
//// ==============================================================================
//int Cmd_cpu(int argc, char *argv[])
//{
//    //
//    // Print some header text.
//    //
//    UARTprintf("ARM Cortex-M4F %u MHz - ",SysCtlClockGet() / 1000000);
//    UARTprintf("%2u%% utilization\n", (g_ulCPUUsage+32768) >> 16);
//
//    // Return success.
//    return(0);
//}

//
//// ==============================================================================
//// This function implements the "free" command.  It prints the free memory.
//// ==============================================================================
//int Cmd_free(int argc, char *argv[])
//{
//    //
//    // Print some header text.
//    //
//    UARTprintf("%d bytes free\n", xPortGetFreeHeapSize());
//
//    // Return success.
//    return(0);
//}

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

int
Cmd_fault(int argc, char *argv[])
{
	*(int*)(0x40001) = 1; /* error logging test... */

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
//    { "cpu",      Cmd_cpu,      "Show CPU utilization" },
//    { "free",     Cmd_free,     "Report free memory" },
//    { "mnt",      Cmd_mnt,      "Mount the SD card" },
//    { "umnt",     Cmd_umnt,     "Unount the SD card" },
//    { "ls",       Cmd_ls,       "Display list of files" },
//    { "chdir",    Cmd_cd,       "Change directory" },
//    { "cd",       Cmd_cd,       "alias for chdir" },
//    { "mkdir",    Cmd_mkdir,    "make a directory" },
//    { "rm",       Cmd_rm,       "Remove file" },
//    { "write",    Cmd_write,    "Write some text to a file" },
//    { "mkfs",     Cmd_mkfs,     "Make filesystem" },
//    { "pwd",      Cmd_pwd,      "Show current working directory" },
//    { "cat",      Cmd_cat,      "Show contents of a text file" },
    { "fault",    Cmd_fault,    "Trigger a hard fault" },

#if ( configUSE_TRACE_FACILITY == 1 )
	{ "tasks",    Cmd_tasks,     "Report stats of all tasks" },
#endif
	{ "pb",    Cmd_protobuftest,     "Test simple protobuf" },

    { 0, 0, 0 }
};

//#include "fault.h"
//static void checkFaults() {
//	faultInfo f;
//	memcpy( (void*)&f, SHUTDOWN_MEM, sizeof(f) );
//	if( f.magic == SHUTDOWN_MAGIC ) {
//		faultPrinter(&f);
//	}
//}

// ==============================================================================
// This is the UARTTask.  It handles command lines received from the RX IRQ.
// ==============================================================================
extern xSemaphoreHandle g_xRxLineSemaphore;
void UARTStdioIntHandler(void);

void vUARTTask( void *pvParameters )
{
    char    cCmdBuf[64];
    int     nStatus;

    //
    // Initialize the UART for console I/O.
    //
    UARTStdioInit(0);

    UARTIntRegister(UARTA0_BASE, UARTStdioIntHandler);

	UARTprintf("\n\nFreeRTOS %s\n",
		tskKERNEL_VERSION_NUMBER);
	UARTprintf("\n? for help\n");
	UARTprintf("> ");

	//checkFaults();

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
