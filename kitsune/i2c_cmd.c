#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <assert.h>
#include <stdint.h>

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "i2c_if.h"
#include "uartstdio.h"

#define FAILURE                 -1
#define SUCCESS                 0

#define RETERR_IF_TRUE(condition) {if(condition) return FAILURE;}
#define RET_IF_ERR(Func)          {int iRetVal = (Func); \
                                   if (SUCCESS != iRetVal) \
                                     return  iRetVal;}

//*****************************************************************************
//
//! Display the buffer contents over I2C
//!
//! \param  pucDataBuf is the pointer to the data store to be displayed
//! \param  ucLen is the length of the data to be displayed
//!
//! \return none
//!
//*****************************************************************************
static void
DisplayBuffer(unsigned char *pucDataBuf, unsigned char ucLen)
{
    unsigned char ucBufIndx = 0;
    UARTprintf("Read contents");
    UARTprintf("\n\r");
    while(ucBufIndx < ucLen)
    {
        UARTprintf(" 0x%x, ", pucDataBuf[ucBufIndx]);
        ucBufIndx++;
        if((ucBufIndx % 8) == 0)
        {
            UARTprintf("\n\r");
        }
    }
    UARTprintf("\n\r");
}

int Cmd_i2c_read(int argc, char *argv[]) {
    unsigned char ucDevAddr, ucLen;
    unsigned char aucDataBuf[256];
    char *pcErrPtr;
    int iRetVal;

    if( argc != 3 ) {
        UARTprintf("read  <dev_addr> <rdlen> \n\r\t - Read data frpm the specified i2c device\n\r");
        return FAILURE;
    }

    // Get the device address
    //
    ucDevAddr = (unsigned char)strtoul(argv[2], &pcErrPtr, 16);
    //
    // Get the length of data to be read
    //
    ucLen = (unsigned char)strtoul(argv[3], &pcErrPtr, 10);

    // Read the specified length of data
    //
    iRetVal = I2CRead(ucDevAddr, aucDataBuf, ucLen);

    if(iRetVal == SUCCESS)
    {
        UARTprintf("I2C Read complete\n\r");
        //
        // Display the buffer over UART on successful write
        //
        DisplayBuffer(aucDataBuf, ucLen);
    }
    else
    {
        UARTprintf("I2C Read failed\n\r");
        return FAILURE;
    }

    return SUCCESS;
}

int Cmd_i2c_writereg(int argc, char *argv[]) {
    unsigned char ucDevAddr, ucRegOffset, ucWrLen;
    unsigned char aucDataBuf[256];
    char *pcErrPtr;
    int iLoopCnt = 0;


    if( argc != 4 ) {
        UARTprintf("writereg <dev_addr> <reg_offset> <wrlen> <<byte0> [<byte1> ... ]> \n\r");
        return FAILURE;
    }
	// Get the device address
	//
	ucDevAddr = (unsigned char)strtoul(argv[2], &pcErrPtr, 16);

	//get the register offset
	ucRegOffset = (unsigned char)strtoul(argv[3], &pcErrPtr, 10);
    aucDataBuf[iLoopCnt] = ucRegOffset;
    iLoopCnt++;
    //
    // Get the length of data to be written
    //
    ucWrLen = (unsigned char)strtoul(argv[4], &pcErrPtr, 10);
    //
    // Get the bytes to be written
    //
    for(; iLoopCnt < ucWrLen + 1; iLoopCnt++)
    {
        //
        // Store the data to be written
        //
        aucDataBuf[iLoopCnt] =
                (unsigned char)strtoul(argv[4], &pcErrPtr, 16);

        ++argv[4];
    }
    //
    // Write the data values.
    //
    RET_IF_ERR(I2CWrite(ucDevAddr,&aucDataBuf[0],ucWrLen+1,1));

    UARTprintf("I2C Write To address complete\n\r");

    return SUCCESS;
}
int Cmd_i2c_readreg(int argc, char *argv[]) {
    unsigned char ucDevAddr, ucRegOffset, ucRdLen;
    unsigned char aucRdDataBuf[256];
    char *pcErrPtr;

    if( argc != 4 ) {
        UARTprintf("readreg <dev_addr> <reg_offset> <rdlen> \n\r");
        return FAILURE;
    }
    //
    // Get the device address
    //
    ucDevAddr = (unsigned char)strtoul(argv[1], &pcErrPtr, 16);
    //
    // Get the register offset address
    //
    ucRegOffset = (unsigned char)strtoul(argv[2], &pcErrPtr, 16);

    //
    // Get the length of data to be read
    //
    ucRdLen = (unsigned char)strtoul(argv[3], &pcErrPtr, 10);

    //
    // Write the register address to be read from.
    // Stop bit implicitly assumed to be 0.
    //
    RET_IF_ERR(I2CWrite(ucDevAddr,&ucRegOffset,1,0));

    vTaskDelay(0);
    //
    // Read the specified length of data
    //
    RET_IF_ERR(I2CRead(ucDevAddr, &aucRdDataBuf[0], ucRdLen));

    UARTprintf("I2C Read From address complete\n\r");
    //
    // Display the buffer over UART on successful readreg
    //
    DisplayBuffer(aucRdDataBuf, ucRdLen);

    return SUCCESS;
}
int Cmd_i2c_write(int argc, char *argv[]) {
    unsigned char ucDevAddr, ucStopBit, ucLen;
    unsigned char aucDataBuf[256];
    char *pcErrPtr;
    int iRetVal, iLoopCnt;

    if( argc != 4 ) {
        UARTprintf("write <dev_addr> <wrlen> <<byte0>[<byte1> ... ]>\n\r");
        return FAILURE;
    }

    //
    // Get the device address
    //
    ucDevAddr = (unsigned char)strtoul(argv[1], &pcErrPtr, 16);
    //
    // Get the length of data to be written
    //
    ucLen = (unsigned char)strtoul(argv[2], &pcErrPtr, 10);
    //RETERR_IF_TRUE(ucLen > sizeof(aucDataBuf));

    for(iLoopCnt = 0; iLoopCnt < ucLen; iLoopCnt++)
    {
        //
        // Store the data to be written
        //
        aucDataBuf[iLoopCnt] =
                (unsigned char)strtoul(argv[3], &pcErrPtr, 16);
        ++argv[3];
    }
    //
    // Get the stop bit
    //
    ucStopBit = (unsigned char)strtoul(argv[3], &pcErrPtr, 10);
    //
    // Write the data to the specified address
    //
    iRetVal = I2CWrite(ucDevAddr, aucDataBuf, ucLen, ucStopBit);
    if(iRetVal == SUCCESS)
    {
        UARTprintf("I2C Write complete\n\r");
    }
    else
    {
        UARTprintf("I2C Write failed\n\r");
        return FAILURE;
    }

    return SUCCESS;

}

int Cmd_readtemp(int argc, char *argv[]) {
    #define TRY_OR_GOTOFAIL(a) if(a!=SUCCESS) { UARTprintf( "fail at %s %s\n\r", __FILE__, __LINE__ ); return FAILURE;}
    unsigned char aucDataBuf[4];
	unsigned char cmd = 0xfe;
	int temp_raw;
	float temp;

	TRY_OR_GOTOFAIL( I2CWrite(0x40, &cmd, 1, 1)  );// reset

    vTaskDelay(10);

    cmd = 0xe3;
    TRY_OR_GOTOFAIL( I2CWrite(0x40, &cmd, 1, 1) );

    vTaskDelay(50);
    TRY_OR_GOTOFAIL( I2CRead(0x40, aucDataBuf, 4) );
    temp_raw = (aucDataBuf[0]<<8) | aucDataBuf[1];
    temp = temp_raw;

    temp *= 175.72;
    temp /= 65536;
    temp -= 46.85;

    UARTprintf( "temp is %f\n\rç", temp );
    return SUCCESS;
}
