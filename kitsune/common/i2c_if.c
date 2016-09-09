//*****************************************************************************
// i2c_if.c
//
// I2C interface APIs. Operates in a polled mode.
//
// Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/ 
// 
// 
//  Redistribution and use in source and binary forms, with or without 
//  modification, are permitted provided that the following conditions 
//  are met:
//
//    Redistributions of source code must retain the above copyright 
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the 
//    documentation and/or other materials provided with the   
//    distribution.
//
//    Neither the name of Texas Instruments Incorporated nor the names of
//    its contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
//  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
//  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
//  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
//  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
//  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
//  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//*****************************************************************************

// Standard includes
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// Driverlib includes
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_ints.h"
#include "hw_i2c.h"
#include "i2c.h"
#include "pin.h"
#include "rom.h"
#include "rom_map.h"
#include "prcm.h"

// Common interface include
#include "i2c_if.h"

#include "FreeRTOS.h"
#include "task.h"


//*****************************************************************************
//                      MACRO DEFINITIONS
//*****************************************************************************
#define I2C_BASE                I2CA0_BASE
#define SYS_CLK                 80000000
#define FAILURE                 -1
#define SUCCESS                 0
#define RETERR_IF_TRUE(condition) {if(condition) return FAILURE;}
#define RET_IF_ERR(Func)          {int iRetVal = (Func); \
                                   if (SUCCESS != iRetVal) \
                                     return  iRetVal;}

//****************************************************************************
//                      LOCAL FUNCTION DEFINITIONS                          
//****************************************************************************
static int I2CTransact(unsigned long ulCmd);


//****************************************************************************
//
//! Invokes the transaction over I2C
//!
//! \param ulCmd is the command to be executed over I2C
//! 
//! This function works in a polling mode,
//!    1. Initiates the transfer of the command.
//!    2. Waits for the I2C transaction completion
//!    3. Check for any error in transaction
//!    4. Clears the master interrupt
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
#include "FreeRTOS.h"
#include "task.h"
#include "uartstdio.h"
#include "interrupt.h"
#include "kit_assert.h"

#include "hw_gpio.h"
#include "hw_types.h"
#include "gpio.h"

#include "hw_memmap.h"
#include "pin.h"
#include "rom.h"
#include "rom_map.h"
#include "prcm.h"
static volatile TaskHandle_t i2c_task;
extern volatile bool booted;
#define I2C_FAILURE 0

static void i2c_int() {
	signed long xHigherPriorityTaskWoken;
	unsigned int sts;
	traceISR_ENTER();
	sts = I2CMasterIntStatusEx(I2C_BASE, false);
    I2CMasterIntClearEx(I2C_BASE,I2C_MASTER_INT_DATA);

	if( I2C_MASTER_INT_DATA == (sts & I2C_MASTER_INT_DATA) ) {
		vTaskNotifyGiveFromISR( i2c_task, &xHigherPriorityTaskWoken );
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
	traceISR_EXIT();
}

void checki2c() {
	int pulses = 0;

	//
	// Configure PIN_02 for GPIOOutput 11
	//

	PinTypeGPIO(PIN_04, PIN_MODE_0, false);
	GPIODirModeSet(GPIOA1_BASE, 0x20, GPIO_DIR_MODE_IN);

	//
	// Configure PIN_01 for GPIOOutput 10 open drain
	//
	PinTypeGPIO(PIN_01, PIN_MODE_0, true);
	GPIODirModeSet(GPIOA1_BASE, 0x4, GPIO_DIR_MODE_OUT);

	TickType_t start = xTaskGetTickCount();
	while ( GPIOPinRead(GPIOA1_BASE, 0x20) == 0 && ++pulses < 32 ) {
		GPIOPinWrite(GPIOA1_BASE, 0x4, 0); //pulse the clock line...
		vTaskDelay(1);
		GPIOPinWrite(GPIOA1_BASE, 0x4, 1);
		vTaskDelay(2);
	}
	GPIODirModeSet(GPIOA1_BASE, 0x4, GPIO_DIR_MODE_IN);

	if( pulses ) {
		LOGE("pulsed %d sda %x scl %x\r\n", pulses, GPIOPinRead(GPIOA1_BASE, 0x20), GPIOPinRead(GPIOA1_BASE, 0x4));
	}
#if 0
	// if the lines stay low, we have failed
	if( booted
		&& ( GPIOPinRead(GPIOA1_BASE, 0x20) == 0
		  || GPIOPinRead(GPIOA1_BASE, 0x4) == 0 ) ) {
		assert( I2C_FAILURE );
	}
#endif
	//SDA is now high again, go back to i2c controller...
	PinTypeI2C(PIN_01, PIN_MODE_1);
	PinTypeI2C(PIN_04, PIN_MODE_5);
	int err = I2CMasterErr(I2C_BASE);
    if( err != I2C_MASTER_ERR_NONE) {
    	LOGE("i2c err %x\n", err );
		vTaskDelay(2);
		I2CMasterControl(I2C_BASE, 0x00000004); //send a stop...
    }
	vTaskDelay(2);
}
static int
I2CTransact(unsigned long ulCmd)
{
	int rval = SUCCESS;
//	unsigned long prio = uxTaskPriorityGet(NULL);

	I2CMasterIntClearEx(I2C_BASE,I2C_MASTER_INT_DATA);
	i2c_task =  xTaskGetCurrentTaskHandle();
    I2CIntRegister(I2C_BASE, i2c_int);
    I2CMasterIntEnableEx(I2C_BASE, I2C_MASTER_INT_DATA);
    //
    // Set the time-out. Not to be used with breakpoints.
    //
    I2CMasterTimeoutSet(I2C_BASE, I2C_TIMEOUT_VAL);
    //
    // Initiate the transfer.
    //
    I2CMasterControl(I2C_BASE, ulCmd);
    //
    // Wait until the current byte has been transferred.
    // Poll on the raw interrupt status.
    //
	if( !ulTaskNotifyTake( pdTRUE, 100 ) ) {
		rval = FAILURE;
	}
	I2CMasterIntDisable(I2C_BASE);
	I2CIntUnregister(I2C_BASE);

	if( rval == FAILURE ) {
		checki2c();
	}
//    vTaskPrioritySet(NULL, prio);

    return rval;
}

//****************************************************************************
//
//! Invokes the I2C driver APIs to write to the specified address
//!
//! \param ucDevAddr is the 7-bit I2C slave address
//! \param pucData is the pointer to the data to be written
//! \param ucLen is the length of data to be written
//! \param ucStop determines if the transaction is followed by stop bit
//! 
//! This function works in a polling mode,
//!    1. Writes the device register address to be written to.
//!    2. In a loop, writes all the bytes over I2C
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
int 
I2C_IF_Write(unsigned char ucDevAddr,
         unsigned char *pucData,
         unsigned char ucLen, 
         unsigned char ucStop)
{
    RETERR_IF_TRUE(pucData == NULL);
    RETERR_IF_TRUE(ucLen == 0);
    //
    // Set I2C codec slave address
    //
    I2CMasterSlaveAddrSet(I2C_BASE, ucDevAddr, false);
    //
    // Write the first byte to the controller.
    //
    I2CMasterDataPut(I2C_BASE, *pucData);
    //
    // Initiate the transfer.
    //
    RET_IF_ERR(I2CTransact(I2C_MASTER_CMD_BURST_SEND_START));
    //
    // Decrement the count and increment the data pointer
    // to facilitate the next transfer
    //
    ucLen--;
    pucData++;
    //
    // Loop until the completion of transfer or error
    //
    while(ucLen)
    {
        //
        // Write the next byte of data
        //
        I2CMasterDataPut(I2C_BASE, *pucData);
        //
        // Transact over I2C to send byte
        //
        RET_IF_ERR(I2CTransact(I2C_MASTER_CMD_BURST_SEND_CONT));
        //
        // Decrement the count and increment the data pointer
        // to facilitate the next transfer
        //
        ucLen--;
        pucData++;
    }
    //
    // If stop bit is to be sent, send it.
    //
    if(ucStop == true)
    {
        RET_IF_ERR(I2CTransact(I2C_MASTER_CMD_BURST_SEND_STOP));
    }

    return SUCCESS;
}

//****************************************************************************
//
//! Invokes the I2C driver APIs to read from the device. This assumes the 
//! device local address to read from is set using the I2CWrite API.
//!
//! \param ucDevAddr is the 7-bit I2C slave address
//! \param pucData is the pointer to the read data to be placed
//! \param ucLen is the length of data to be read
//! 
//! This function works in a polling mode,
//!    1. Writes the device register address to be written to.
//!    2. In a loop, reads all the bytes over I2C
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
int 
I2C_IF_Read(unsigned char ucDevAddr,
        unsigned char *pucData,
        unsigned char ucLen)
{
    unsigned long ulCmdID;

    RETERR_IF_TRUE(pucData == NULL);
    RETERR_IF_TRUE(ucLen == 0);
    //
    // Set I2C codec slave address
    //
    I2CMasterSlaveAddrSet(I2C_BASE, ucDevAddr, true);
    //
    // Check if its a single receive or burst receive
    //
    if(ucLen == 1)
    {
        //
        // Configure for a single receive
        //
        ulCmdID = I2C_MASTER_CMD_SINGLE_RECEIVE;
    }
    else
    {
        //
        // Initiate a burst receive sequence
        //
        ulCmdID = I2C_MASTER_CMD_BURST_RECEIVE_START;
    }
    //
    // Initiate the transfer.
    //
    RET_IF_ERR(I2CTransact(ulCmdID));
    //
    // Decrement the count and increment the data pointer
    // to facilitate the next transfer
    //
    ucLen--;
    //
    // Loop until the completion of reception or error
    //
    while(ucLen)
    {
        //
        // Receive the byte over I2C
        //
        *pucData = I2CMasterDataGet(I2C_BASE);
        //
        // Decrement the count and increment the data pointer
        // to facilitate the next transfer
        //
        ucLen--;
        pucData++;
        if(ucLen)
        {
            //
            // Continue the reception
            //
            RET_IF_ERR(I2CTransact(I2C_MASTER_CMD_BURST_RECEIVE_CONT));
        }
        else
        {
            //
            // Complete the last reception
            //
            RET_IF_ERR(I2CTransact(I2C_MASTER_CMD_BURST_RECEIVE_FINISH));
        }
    }
    //
    // Receive the byte over I2C
    //
    *pucData = I2CMasterDataGet(I2C_BASE);

    return SUCCESS;
}

//****************************************************************************
//
//! Invokes the I2C driver APIs to read from a specified address the device. 
//! This assumes the device local address to be of 8-bit. For other 
//! combinations use I2CWrite followed by I2CRead.
//!
//! \param ucDevAddr is the 7-bit I2C slave address
//! \param pucWrDataBuf is the pointer to the data to be written (reg addr)
//! \param ucWrLen is the length of data to be written
//! \param pucRdDataBuf is the pointer to the read data to be placed
//! \param ucRdLen is the length of data to be read
//! 
//! This function works in a polling mode,
//!    1. Writes the data over I2C (device register address to be read from).
//!    2. In a loop, reads all the bytes over I2C
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
int 
I2C_IF_ReadFrom(unsigned char ucDevAddr,
            unsigned char *pucWrDataBuf,
            unsigned char ucWrLen,
            unsigned char *pucRdDataBuf,
            unsigned char ucRdLen)
{
    //
    // Write the register address to be read from.
    // Stop bit implicitly assumed to be 0.
    //
    RET_IF_ERR(I2C_IF_Write(ucDevAddr,pucWrDataBuf,ucWrLen,0));
    //
    // Read the specified length of data
    //
    RET_IF_ERR(I2C_IF_Read(ucDevAddr, pucRdDataBuf, ucRdLen));

    return SUCCESS;
}

//****************************************************************************
//
//! Enables and configures the I2C peripheral
//!
//! \param ulMode is the mode configuration of I2C
//! The parameter \e ulMode is one of the following
//! - \b I2C_MASTER_MODE_STD for 100 Kbps standard mode.
//! - \b I2C_MASTER_MODE_FST for 400 Kbps fast mode.
//! 
//! This function works in a polling mode,
//!    1. Powers ON the I2C peripheral.
//!    2. Configures the I2C peripheral
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
int 
I2C_IF_Open(unsigned long ulMode)
{
    //
    // Enable I2C Peripheral
    //           
    //HwSemaphoreLock(HWSEM_I2C, HWSEM_WAIT_FOR_EVER);
    PRCMPeripheralClkEnable(PRCM_I2CA0, PRCM_RUN_MODE_CLK);
    PRCMPeripheralReset(PRCM_I2CA0);

    //
    // Configure I2C module in the specified mode
    //
    switch(ulMode)
    {
        case I2C_MASTER_MODE_STD:       /* 100000 */
            I2CMasterInitExpClk(I2C_BASE,SYS_CLK,false);
            break;

        case I2C_MASTER_MODE_FST:       /* 400000 */
            I2CMasterInitExpClk(I2C_BASE,SYS_CLK,true);
            break;

        default:
            I2CMasterInitExpClk(I2C_BASE,SYS_CLK,true);
            break;
    }
    //
    // Disables the multi-master mode
    //
    //I2CMasterDisable(I2C_BASE);
    
    return SUCCESS;
}

//****************************************************************************
//
//! Disables the I2C peripheral
//!
//! \param None
//! 
//! This function works in a polling mode,
//!    1. Powers OFF the I2C peripheral.
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
int 
I2C_IF_Close()
{
    //
    // Power OFF the I2C peripheral
    //
    PRCMPeripheralClkDisable(PRCM_I2CA0, PRCM_RUN_MODE_CLK);

    return SUCCESS;
}
