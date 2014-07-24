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

#include "wifi_cmd.h"
#include "i2c_cmd.h"
#include "dust_cmd.h"

#define NUM_LOGS 72

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
//    UARTprintf("ARM Cortex-M4F %u MHz - ",configCPU_CLOCK_HZ / 1000000);
//    UARTprintf("%2u%% utilization\n", (g_ulCPUUsage+32768) >> 16);
//
//    // Return success.
//    return(0);
//}

// ==============================================================================
// This function implements the "free" command.  It prints the free memory.
// ==============================================================================
int Cmd_free(int argc, char *argv[]) {
	//
	// Print some header text.
	//
	UARTprintf("%d bytes free\n", xPortGetFreeHeapSize());

	// Return success.
	return (0);
}

#include "fs.h"

int Cmd_fs_write(int argc, char *argv[]) {
	//
	// Print some header text.
	//
	unsigned long tok;
	int err;
	long hndl, bytes;
	SlFsFileInfo_t info;

	sl_FsGetInfo(argv[1], tok, &info);

	if (sl_FsOpen(argv[1],
	FS_MODE_OPEN_WRITE, &tok, &hndl)) {
		UARTprintf("error opening file, trying to create\n");

		if (sl_FsOpen(argv[1],
				FS_MODE_OPEN_CREATE(65535, _FS_FILE_OPEN_FLAG_COMMIT), &tok,
				&hndl)) {
			UARTprintf("error opening for write\n");
			return -1;
		}
	}

	bytes = sl_FsWrite(hndl, info.FileLen, argv[2], strlen(argv[2]));
	UARTprintf("wrote to the file %d bytes\n", bytes);

	sl_FsClose(hndl, 0, 0, 0);

	// Return success.
	return (0);
}
int Cmd_fs_read(int argc, char *argv[]) {
	//
	// Print some header text.
	//
#define minval( a,b ) a < b ? a : b
#define BUF_SZ 600
	unsigned long tok;
	long hndl, err, bytes, i;
	SlFsFileInfo_t info;
	char buffer[BUF_SZ];

	sl_FsGetInfo(argv[1], tok, &info);

	if (err = sl_FsOpen(argv[1], FS_MODE_OPEN_READ, &tok, &hndl)) {
		UARTprintf("error opening for read %d\n", err);
		return -1;
	}

	if (bytes = sl_FsRead(hndl, 0, buffer, minval(info.FileLen, BUF_SZ))) {
		UARTprintf("read %d bytes\n", bytes);
	}

	sl_FsClose(hndl, 0, 0, 0);

	for(i=0;i<bytes;++i) {
		UARTprintf("%x", buffer[i]);
	}

	// Return success.
	return (0);
}
int Cmd_fs_delete(int argc, char *argv[]) {
	//
	// Print some header text.
	//
	int err;

	if (err = sl_FsDel(argv[1], 0)) {
		UARTprintf("error %d\n", err);
		return -1;
	}

	// Return success.
	return (0);
}

#define BUF_SZ 10
int Cmd_readout_data(int argc, char *argv[]) {
	long hndl, err, bytes, i,j,fail;

	typedef struct {
		int time, light, temp, humid, dust;
	} data_t;

	data_t data[BUF_SZ];
	char buf[10];

	for( j=0;j<NUM_LOGS;++j) {
		snprintf( buf, 10, "%d", j);
		if (err = sl_FsOpen(buf, FS_MODE_OPEN_READ, NULL, &hndl)) {
			UARTprintf("error opening for read %d\n", err);
		}

		if (bytes = sl_FsRead(hndl, 0, data, sizeof(data))) {
			//UARTprintf("read %d bytes\n", bytes);
		}
		sl_FsClose(hndl, 0, 0, 0);

		if( bytes != sizeof(data) ) {
			continue;
		}

		fail = 0;
		//UARTprintf("cnt,time,light,temp,humid,dust\n", i, data[i].time, data[i].light, data[i].temp, data[i].humid, data[i].dust );
		for( i=0; i!=BUF_SZ;++i ) {
			UARTprintf("%d\t%d\t%d\t%d\t%d\t%d\n", i, data[i].time, data[i].light, data[i].temp, data[i].humid, data[i].dust );
			if( !send_data( &data[i] ) ) {
				fail=1;
			}
			vTaskDelay(10);
		}
		if( !fail ) {
			sl_FsDel( buf, NULL );
		}
	}
	return 0;
}

int thead_sensor_poll(void* unused) {

	//
	// Print some header text.
	//

	data_t data[BUF_SZ];

	int i=0;
	int fn=0;

	unsigned long tok;
	long hndl;
	int bytes, written;

	while( !(sl_status&HAS_IP ) ) {vTaskDelay(100);}

	get_light(); //first reading is always buggy

	while (1) {
#define SENSOR_RATE 60
		portTickType now = xTaskGetTickCount();
		unsigned long ntp=-1;
		static unsigned long last_ntp = -1;

		if( last_ntp == -1 ) {
			ntp = last_ntp = unix_time();
		} else if( last_ntp != -1) {
			ntp = last_ntp+SENSOR_RATE;
			last_ntp = ntp;
		}

		data[i].time = ntp != -1 ? ntp : now/1000;
		data[i].dust = get_dust();
		data[i].light = get_light();
		data[i].humid = get_humid();
		data[i].temp = get_temp();
		UARTprintf("cnt %d\ttime %d\tlight %d\ttemp %d\thumid %d\tdust %d\n", i, data[i].time, data[i].light, data[i].temp, data[i].humid, data[i].dust );

		while(i >= 0) {
		    if( send_data( &data[i] ) != 0 ) {
		    	--i;
		    } else {
		    	break;
		    }
		}
//		send_data_pb( &data[i] );

#if 1
		if (++i == BUF_SZ) {
			char fn_str[10];

			snprintf(fn_str, 10, "%d", fn);
			sl_FsOpen(fn_str, FS_MODE_OPEN_CREATE(1024, _FS_FILE_OPEN_FLAG_COMMIT), &tok, &hndl);

			written = 0;
			while (written != sizeof(data)) {
				bytes = sl_FsWrite(hndl, 0, ((char*)data)+written, sizeof(data));
				written += bytes;
				UARTprintf("wrote to the file %d bytes\n", bytes);
			}
			UARTprintf("finished, wrote %d bytes\n", written);

			sl_FsClose(hndl, 0, 0, 0);
			if( ++fn > NUM_LOGS ) {
				fn = 0;
			}

			i=0;
		}
#endif

		UARTprintf("delay...\n");
		vTaskDelayUntil(&now, SENSOR_RATE * configTICK_RATE_HZ);
	}

}

int Cmd_sensor_poll(int argc, char *argv[]) {
	xTaskCreate( thead_sensor_poll, "pollTask", 10*1024/4, NULL, 2, NULL );
// Return success.
	return (0);
}

// ==============================================================================
// This function implements the "tasks" command.  It prints a list of the
// current FreeRTOS tasks with information about status, stack usage, etc.
// ==============================================================================
#if ( configUSE_TRACE_FACILITY == 1 )

int Cmd_tasks(int argc, char *argv[]) {
	signed char* pBuffer;

	pBuffer = pvPortMalloc(1024);
	vTaskList(pBuffer);
	UARTprintf("\t\t\t\t\tUnused\nTaskName\t\tStatus\tPri\tStack\tTask ID\n");
	UARTprintf("=======================================================");
	UARTprintf("%s", pBuffer);

	vPortFree(pBuffer);
	return 0;
}

#endif /* configUSE_TRACE_FACILITY */

// ==============================================================================
// This function implements the "help" command.  It prints a simple list of the
// available commands with a brief description.
// ==============================================================================
int Cmd_help(int argc, char *argv[]) {
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
	while (pEntry->pcCmd) {
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
	return (0);
}

int Cmd_protobuftest(int argc, char *argv[]) {
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
		if (!status) {
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
		if (!status) {
			UARTprintf("Decoding failed: %s\n", PB_GET_ERROR(&stream));
			return 1;
		}

		/* Print the data contained in the message. */
		UARTprintf("lucky number %d!\n", message.lucky_number);
	}

	return 0;
}

int Cmd_fault(int argc, char *argv[]) {
	*(int*) (0x40001) = 1; /* error logging test... */

	return (0);
}

// ==============================================================================
// This is the table that holds the command names, implementing functions, and
// brief description.
// ==============================================================================
tCmdLineEntry g_sCmdTable[] = {
		{ "help", Cmd_help, "Display list of commands" }, { "?", Cmd_help,
				"alias for help" },
//    { "cpu",      Cmd_cpu,      "Show CPU utilization" },
		{ "free", Cmd_free, "Report free memory" }, { "connect", Cmd_connect,
				"Connect to an AP" }, { "ping", Cmd_ping, "Ping a server" },
				{ "time", Cmd_time, "get ntp time" },{
				"status", Cmd_status, "status of simple link" },
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
		{ "fault", Cmd_fault, "Trigger a hard fault" }, { "i2crd", Cmd_i2c_read,
				"i2c read" }, { "i2cwr", Cmd_i2c_write, "i2c write" }, {
				"i2crdrg", Cmd_i2c_readreg, "i2c readreg" }, { "i2cwrrg",
				Cmd_i2c_writereg, "i2c_writereg" },

		{ "humid", Cmd_readhumid, "i2 read humid" }, { "temp", Cmd_readtemp,
				"i2 read temp" }, { "light", Cmd_readlight, "i2 read light" }, {
				"proximity", Cmd_readproximity, "i2 read proximity" },
#if ( configUSE_TRACE_FACILITY == 1 )
		{ "tasks", Cmd_tasks, "Report stats of all tasks" },
#endif
		{ "pb", Cmd_protobuftest, "Test simple protobuf" },

		{ "dust", Cmd_dusttest, "dust sensor test" },

		{ "fswr", Cmd_fs_write, "fs write" },
		{ "fsrd", Cmd_fs_read, "fs read" },
		{ "fsdl", Cmd_fs_delete, "fs delete" },

		{ "poll", Cmd_sensor_poll, "poll sensors" },
		{ "readout", Cmd_readout_data, "read out sensor data log" },
		{ "sl", Cmd_sl, "start smart config" },
		{ "mode", Cmd_mode, "set the ap/station mode" },

		{ 0, 0, 0 } };

//#include "fault.h"
//static void checkFaults() {
//    faultInfo f;
//    memcpy( (void*)&f, SHUTDOWN_MEM, sizeof(f) );
//    if( f.magic == SHUTDOWN_MAGIC ) {
//        faultPrinter(&f);
//    }
//}

// ==============================================================================
// This is the UARTTask.  It handles command lines received from the RX IRQ.
// ==============================================================================
extern xSemaphoreHandle g_xRxLineSemaphore;
void UARTStdioIntHandler(void);

#define KIT_VER "1"

void vUARTTask(void *pvParameters) {
	char cCmdBuf[64];
	int nStatus;

	//
	// Initialize the UART for console I/O.
	//
	UARTStdioInit(0);

	UARTIntRegister(UARTA0_BASE, UARTStdioIntHandler);

	UARTprintf("\n\nFreeRTOS %s, %s\n",
	tskKERNEL_VERSION_NUMBER, KIT_VER);
	UARTprintf("\n? for help\n");
	UARTprintf("> ");

	sl_mode = sl_Start(NULL, NULL, NULL);

	vTaskDelay(1000);
	if(sl_mode == ROLE_AP || !sl_status) {
		Cmd_sl(0,0);
	}

	xTaskCreate( thead_sensor_poll, "pollTask", 10*1024/4, NULL, 2, NULL );

	//checkFaults();

	/* Loop forever */
	while (1) {

		/* Wait for a signal indicating we have an RX line to process */
		xSemaphoreTake(g_xRxLineSemaphore, portMAX_DELAY);

		if (UARTPeek('\r') != -1) {
			/* Read data from the UART and process the command line */
			UARTgets(cCmdBuf, sizeof(cCmdBuf));
			if (ustrlen(cCmdBuf) == 0) {
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
			if (nStatus == CMDLINE_BAD_CMD) {
				UARTprintf("Bad command!\n");
			}

			//
			// Handle the case of too many arguments.
			//
			else if (nStatus == CMDLINE_TOO_MANY_ARGS) {
				UARTprintf("Too many arguments for command processor!\n");
			}

			//
			// Otherwise the command was executed.  Print the error code if one was
			// returned.
			//
			else if (nStatus != 0) {
				UARTprintf("Command returned error code %d\n", nStatus);
			}

			UARTprintf("> ");
		}
	}
}
