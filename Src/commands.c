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

void vMath1( void *pvParameters );
void vMath2( void *pvParameters );

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

// ==============================================================================
// Define colors for the OLED to cycle through */
// ==============================================================================
typedef struct
{
	const char* pName;
	unsigned long	value;
} colorTable_t;

#define	COLOR_TABLE_ENTRY(c) { #c, c }

const colorTable_t g_colors[] =
{
	COLOR_TABLE_ENTRY(ClrAntiqueWhite),  
	COLOR_TABLE_ENTRY(ClrAqua),  
	COLOR_TABLE_ENTRY(ClrBisque),  
	COLOR_TABLE_ENTRY(ClrBlack),  
	COLOR_TABLE_ENTRY(ClrBlue),  
	COLOR_TABLE_ENTRY(ClrBlueViolet),  
	COLOR_TABLE_ENTRY(ClrBrown),  
	COLOR_TABLE_ENTRY(ClrBurlyWood),  
	COLOR_TABLE_ENTRY(ClrCadetBlue),  
	COLOR_TABLE_ENTRY(ClrChartreuse),  
	COLOR_TABLE_ENTRY(ClrChocolate),  
	COLOR_TABLE_ENTRY(ClrCoral),  
	COLOR_TABLE_ENTRY(ClrCornflowerBlue),  
	COLOR_TABLE_ENTRY(ClrCornsilk),  
	COLOR_TABLE_ENTRY(ClrCrimson),  
	COLOR_TABLE_ENTRY(ClrCyan),  
	COLOR_TABLE_ENTRY(ClrDarkBlue),  
	COLOR_TABLE_ENTRY(ClrDarkCyan),  
	COLOR_TABLE_ENTRY(ClrDarkGoldenrod),  
	COLOR_TABLE_ENTRY(ClrDarkGray),  
	COLOR_TABLE_ENTRY(ClrDarkGreen),  
	COLOR_TABLE_ENTRY(ClrDarkKhaki),  
	COLOR_TABLE_ENTRY(ClrDarkMagenta),  
	COLOR_TABLE_ENTRY(ClrDarkOliveGreen),  
	COLOR_TABLE_ENTRY(ClrDarkOrange),  
	COLOR_TABLE_ENTRY(ClrDarkOrchid),  
	COLOR_TABLE_ENTRY(ClrDarkRed),  
	COLOR_TABLE_ENTRY(ClrDarkSalmon),  
	COLOR_TABLE_ENTRY(ClrDarkSeaGreen),  
	COLOR_TABLE_ENTRY(ClrDarkSlateBlue),  
	COLOR_TABLE_ENTRY(ClrDarkSlateGray),  
	COLOR_TABLE_ENTRY(ClrDarkTurquoise),  
	COLOR_TABLE_ENTRY(ClrDarkViolet),  
	COLOR_TABLE_ENTRY(ClrDeepPink),  
	COLOR_TABLE_ENTRY(ClrDeepSkyBlue),  
	COLOR_TABLE_ENTRY(ClrDimGray),  
	COLOR_TABLE_ENTRY(ClrDodgerBlue),  
	COLOR_TABLE_ENTRY(ClrFireBrick),  
	COLOR_TABLE_ENTRY(ClrFloralWhite),  
	COLOR_TABLE_ENTRY(ClrForestGreen),  
	COLOR_TABLE_ENTRY(ClrFuchsia),  
	COLOR_TABLE_ENTRY(ClrGainsboro),  
	COLOR_TABLE_ENTRY(ClrGhostWhite),  
	COLOR_TABLE_ENTRY(ClrGold),  
	COLOR_TABLE_ENTRY(ClrGoldenrod),  
	COLOR_TABLE_ENTRY(ClrGray),  
	COLOR_TABLE_ENTRY(ClrGreen),  
	COLOR_TABLE_ENTRY(ClrGreenYellow),  
	COLOR_TABLE_ENTRY(ClrHoneydew),  
	COLOR_TABLE_ENTRY(ClrHotPink),  
	COLOR_TABLE_ENTRY(ClrIndianRed),  
	COLOR_TABLE_ENTRY(ClrIndigo),  
	COLOR_TABLE_ENTRY(ClrIvory),  
	COLOR_TABLE_ENTRY(ClrKhaki),  
	COLOR_TABLE_ENTRY(ClrLavender),  
	COLOR_TABLE_ENTRY(ClrLavenderBlush),  
	COLOR_TABLE_ENTRY(ClrLawnGreen),  
	COLOR_TABLE_ENTRY(ClrLemonChiffon),  
	COLOR_TABLE_ENTRY(ClrLightBlue),  
	COLOR_TABLE_ENTRY(ClrLightCoral),  
	COLOR_TABLE_ENTRY(ClrLightCyan),  
	COLOR_TABLE_ENTRY(ClrLightGoldenrodYellow), 
	COLOR_TABLE_ENTRY(ClrLightGreen),  
	COLOR_TABLE_ENTRY(ClrLightGrey),  
	COLOR_TABLE_ENTRY(ClrLightPink),  
	COLOR_TABLE_ENTRY(ClrLightSalmon),  
	COLOR_TABLE_ENTRY(ClrLightSeaGreen),  
	COLOR_TABLE_ENTRY(ClrLightSkyBlue),  
	COLOR_TABLE_ENTRY(ClrLightSlateGray),  
	COLOR_TABLE_ENTRY(ClrLightSteelBlue),  
	COLOR_TABLE_ENTRY(ClrLightYellow),  
	COLOR_TABLE_ENTRY(ClrLime),  
	COLOR_TABLE_ENTRY(ClrLimeGreen),  
	COLOR_TABLE_ENTRY(ClrLinen),  
	COLOR_TABLE_ENTRY(ClrMagenta),  
	COLOR_TABLE_ENTRY(ClrMaroon),  
	COLOR_TABLE_ENTRY(ClrMediumAquamarine),  
	COLOR_TABLE_ENTRY(ClrMediumBlue),  
	COLOR_TABLE_ENTRY(ClrMediumOrchid),  
	COLOR_TABLE_ENTRY(ClrMediumPurple),  
	COLOR_TABLE_ENTRY(ClrMediumSeaGreen),  
	COLOR_TABLE_ENTRY(ClrMediumSlateBlue),  
	COLOR_TABLE_ENTRY(ClrMediumSpringGreen),  
	COLOR_TABLE_ENTRY(ClrMediumTurquoise),  
	COLOR_TABLE_ENTRY(ClrMediumVioletRed),  
	COLOR_TABLE_ENTRY(ClrMidnightBlue),  
	COLOR_TABLE_ENTRY(ClrMintCream),  
	COLOR_TABLE_ENTRY(ClrMistyRose),  
	COLOR_TABLE_ENTRY(ClrMoccasin),  
	COLOR_TABLE_ENTRY(ClrNavajoWhite),  
	COLOR_TABLE_ENTRY(ClrNavy),  
	COLOR_TABLE_ENTRY(ClrOldLace),  
	COLOR_TABLE_ENTRY(ClrOlive),  
	COLOR_TABLE_ENTRY(ClrOliveDrab),  
	COLOR_TABLE_ENTRY(ClrOrange),  
	COLOR_TABLE_ENTRY(ClrOrangeRed),  
	COLOR_TABLE_ENTRY(ClrOrchid),  
	COLOR_TABLE_ENTRY(ClrPaleGoldenrod),  
	COLOR_TABLE_ENTRY(ClrPaleGreen),  
	COLOR_TABLE_ENTRY(ClrPaleTurquoise),  
	COLOR_TABLE_ENTRY(ClrPaleVioletRed),  
	COLOR_TABLE_ENTRY(ClrPapayaWhip),  
	COLOR_TABLE_ENTRY(ClrPeachPuff),  
	COLOR_TABLE_ENTRY(ClrPeru),  
	COLOR_TABLE_ENTRY(ClrPink),  
	COLOR_TABLE_ENTRY(ClrPlum),  
	COLOR_TABLE_ENTRY(ClrPowderBlue),  
	COLOR_TABLE_ENTRY(ClrPurple),  
	COLOR_TABLE_ENTRY(ClrRed),  
	COLOR_TABLE_ENTRY(ClrRosyBrown),  
	COLOR_TABLE_ENTRY(ClrRoyalBlue),  
	COLOR_TABLE_ENTRY(ClrSaddleBrown),  
	COLOR_TABLE_ENTRY(ClrSalmon),  
	COLOR_TABLE_ENTRY(ClrSandyBrown),  
	COLOR_TABLE_ENTRY(ClrSeaGreen),  
	COLOR_TABLE_ENTRY(ClrSeashell),  
	COLOR_TABLE_ENTRY(ClrSienna),  
	COLOR_TABLE_ENTRY(ClrSilver),  
	COLOR_TABLE_ENTRY(ClrSkyBlue),  
	COLOR_TABLE_ENTRY(ClrSlateBlue),  
	COLOR_TABLE_ENTRY(ClrSlateGray),  
	COLOR_TABLE_ENTRY(ClrSnow),  
	COLOR_TABLE_ENTRY(ClrSpringGreen),  
	COLOR_TABLE_ENTRY(ClrSteelBlue),  
	COLOR_TABLE_ENTRY(ClrTan),  
	COLOR_TABLE_ENTRY(ClrTeal),  
	COLOR_TABLE_ENTRY(ClrThistle),  
	COLOR_TABLE_ENTRY(ClrTomato),  
	COLOR_TABLE_ENTRY(ClrTurquoise),  
	COLOR_TABLE_ENTRY(ClrViolet),  
	COLOR_TABLE_ENTRY(ClrWheat),  
	COLOR_TABLE_ENTRY(ClrWhite),  
	COLOR_TABLE_ENTRY(ClrWhiteSmoke),  
	COLOR_TABLE_ENTRY(ClrYellow),  
	COLOR_TABLE_ENTRY(ClrYellowGreen),
	{ NULL, 0 } 
};  

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
// This function implements the "color" command.  It controls the color on the
// OLED (including cycling) and can display a list of known colors.
// ==============================================================================
int Cmd_color(int argc, char *argv[])
{
	int		c;
	
    //
    // Test if an argument was given
    //
    if (argc == 1)
    {
    	UARTprintf("Set the OLED color mode.  You can specify:\n");
    	UARTprintf("   color colorName - set specific color\n");
    	UARTprintf("   color checker - display random checker board\n");
    	UARTprintf("   color cycle - cycle through all colors\n");
    	UARTprintf("   color list - list names of known colors\n");
    	UARTprintf("   color next - cycle to next color in table\n");
    	
    	return 0;
    }
    else
    {
    	if (ustrcmp(argv[1], "cycle") == 0)
    	{
    		g_pTaskParams->color = -1;
    		return 0;
    	}
    	else if (ustrcmp(argv[1], "checker") == 0)
    	{
    		g_pTaskParams->color = -3;
    		return 0;
    	}
    	else if (ustrcmp(argv[1], "next") == 0)
    	{
    		c = g_pTaskParams->color;
    		g_pTaskParams->color = -2;
    		
    		/* Print the name of the next color */
    		if (c >= 0)
    		{
    			/* Increment to next color name */
    			c++;
    			if (g_colors[c].pName == NULL)
    				c = 0;
    			UARTprintf("%s\n", &g_colors[c].pName[3]);
    		}
    		return 0;
    	}
    	else if (ustrcmp(argv[1], "list") == 0)
    	{
    		/* Print a list of colors */
    		for (c = 0; g_colors[c].pName != NULL; c++)
    		{
    			UARTprintf("%s, ", &g_colors[c].pName[3]);
    			if (((c-3) % 4) == 0)
    				UARTprintf("\n");
    			if (c == (4 * 20)-1)
    			{
    				UARTprintf("\nspace or CTRL-C ...");
    				while (UARTPeek(' ') == -1)
    				{
    					if (UARTPeek('\x03') != -1)
    					{
    						// CTRL-C detected
    						UARTFlushRx();
    						UARTprintf("^C\n");
    						return 0;
    					}
    				}
    				UARTprintf("\n");
    				UARTFlushRx();
    			}
    		}
   			UARTprintf("\n");
    		return 0;
    	}
    	else
    	{
    		unsigned char len1 = ustrlen(argv[1]);
    		unsigned char maxLen, refLen;
    		
    		/* Find the color in the table */
    		for (c = 0; g_colors[c].pName != NULL; c++)
    		{
    			refLen = ustrlen(g_colors[c].pName)-3;
    			maxLen = len1 > refLen ? len1 : refLen;
    			if (ustrnicmp(argv[1], &g_colors[c].pName[3], maxLen) == 0)
    			{
    				/* Set the color to this color and return */
					g_pTaskParams->color = c;
					return 0;    				
    			}
    		}
    		
    		UARTprintf("Unknown color - use 'color list'\n");
    		return 0;
    	}
    }
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
        UARTprintf("%s%s\n", pEntry->pcCmd, pEntry->pcHelp);

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

// ==============================================================================
// This is the 'fputest' command.  It creates 2 new tasks that perform floating
// point math using the hardware FPU.  It is used to validate the FPU context
// switch logic.
// ==============================================================================
int Cmd_fputest(int argc, char *argv[])
{
	portBASE_TYPE	seconds;
		
	if (argc == 1)
	{
		UARTprintf("Running for 5 seconds (specify seconds on command line)\n");
		seconds = 5;
	}
	else
	{
		seconds = ustrtoul(argv[1], NULL, 10);
		UARTprintf("Running for %d seconds\n", seconds);
	}
	
	/* Start the two math tasks */
    xTaskCreate( vMath1, "Math1Task", 90, (void *) seconds, 1, NULL );
    xTaskCreate( vMath2, "Math2Task", 90, (void *) seconds, 1, NULL );
    
    return 0;
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

// ==============================================================================
// This is the table that holds the command names, implementing functions, and
// brief description.
// ==============================================================================
tCmdLineEntry g_sCmdTable[] =
{
    { "help",     Cmd_help,      "     : Display list of commands" },
    { "?",        Cmd_help,      "        : alias for help" },
    { "cpu",      Cmd_cpu,       "      : Show CPU utilization" },
    { "color",    Cmd_color,     "    : Paint the OLED a specific color" },
    { "fputest",  Cmd_fputest,   "  : Execute FPU context switch test" },
    { "free",     Cmd_free,      "     : Report free memory" },
#if ( configUSE_TRACE_FACILITY == 1 )
	{ "tasks",    Cmd_tasks,     "    : Report stats of all tasks" },
#endif
	{ "pb",    Cmd_protobuftest,     "    : Test simple protobuf" },

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

// ==============================================================================
// Perform some floating point math to test FPU context switch operation.
// ==============================================================================
void vMath1( void *pvParameters )
{
	portBASE_TYPE	startTick = xTaskGetTickCount();
	portBASE_TYPE	endTick = startTick + (portBASE_TYPE) pvParameters * configTICK_RATE_HZ;
	volatile float		f1, f2, f3;
	volatile int		match;
	portBASE_TYPE		x;
	
	while (xTaskGetTickCount() < endTick)	
	{
		for (x = 0; x < 1000; x++)
		{
			f1 = 2079.1;
			f2 = 17.0;
			
			// Force a context switch to validate FPU registers are preserved
			portYIELD();
			f3 = f1 / f2 - 122.3;
			match = (f3 < 10e-5);
			if (!match)
			{
				UARTprintf("ASSERT: vMath1 float mismatch!\n");
				vTaskDelay(configTICK_RATE_HZ/4);
			}
			assert(match);
		}
		vTaskDelay(5 / portTICK_RATE_MS);	// Give the idle task some priority
	}
	
	/* Exit from our task.  Our port provides a return hook that will
	 * delete our task and wait for the OS to terminate us.
	 * 
	 * NOTE:  This is unique to THIS port only!  Normally you would never
	 *        want to return from a task function, as there would be 
	 *        nowhere to return to.
	 */
}

// ==============================================================================
// Perform some floating point math to test FPU context switch operation.
// ==============================================================================
void vMath2( void *pvParameters )
{
	portBASE_TYPE	startTick = xTaskGetTickCount();
	portBASE_TYPE	endTick = startTick + (portBASE_TYPE) pvParameters * configTICK_RATE_HZ;
	volatile float	f1, f2, f3;
	volatile int	match;
	portBASE_TYPE	x;
	
	while (xTaskGetTickCount() < endTick)	
	{
		for (x = 0; x < 1000; x++)
		{
			f1 = 2.1;
			f2 = 6.93;
			
			// Force a context switch to validate FPU registers are preserved
			portYIELD();
			f3 = f2 / f1 - 3.3;
			match = (f3 < 10e-5);
			if (!match)
			{
				UARTprintf("ASSERT: vMath2 float mismatch!\n");
				vTaskDelay(configTICK_RATE_HZ/4);
			}
			assert(match);
		}
		vTaskDelay(5 / portTICK_RATE_MS);	// Give the idle task some priority
	}
	
	/* Exit from our task.  Our port provides a return hook that will
	 * delete our task and wait for the OS to terminate us.
	 * 
	 * NOTE:  This is unique to THIS port only!  Normally you would never
	 *        want to return from a task function, as there would be 
	 *        nowhere to return to.
	 */
}
