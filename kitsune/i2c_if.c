/******************************************************************************
*
*   Copyright (C) 2013 Texas Instruments Incorporated
*
*   All rights reserved. Property of Texas Instruments Incorporated.
*   Restricted rights to use, duplicate or disclose this code are
*   granted through contract.
*
*   The program may not be used without the written permission of
*   Texas Instruments Incorporated or against the terms and conditions
*   stipulated in the agreement under which this program has been supplied,
*   and under no circumstances can it be used with non-TI connectivity device.
*
******************************************************************************/
//****************************************************************************
//
// i2c_if.c - I2C interface APIs. Operates in a polled mode.
//
//****************************************************************************
//*****************************************************************************
//
//! \addtogroup i2c_demo
//! @{
//
//*****************************************************************************
#include <stdbool.h>
#include <stdint.h>
#include <hw_types.h>
#include <i2c.h>
#include <hw_memmap.h>
#include <hw_ints.h>
#include <hw_i2c.h>
#include <rom.h>
#include <rom_map.h>
#include <stdlib.h>
#include <prcm.h>
#include <hwspinlock.h>
#include "i2c_if.h"
#include <rom.h>
#include <rom_map.h>
#include <pin.h>
//****************************************************************************
//                      GLOBAL VARIABLES                                   
//****************************************************************************

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
//!    1. Initiates the trasfer of the command.
//!    2. Waits for the I2C transaction completion
//!    3. Check for any error in transaction
//!    4. Clears the master interrupt
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
static int 
I2CTransact(unsigned long ulCmd)
{
	int retires = 0;
    //
    // Clear all interrupts
    //
    MAP_I2CMasterIntClearEx(I2C_BASE,MAP_I2CMasterIntStatusEx(I2C_BASE,false));
    //
    // Set the timeout. Not to be used with breakpoints.
    //
    MAP_I2CMasterTimeoutSet(I2C_BASE, I2C_TIMEOUT_VAL);
    //
    // Initiate the transfer.
    //
    MAP_I2CMasterControl(I2C_BASE, ulCmd);
    //
    // Wait until the current byte has been transferred.
    // Poll on the raw interrupt status.
    //
    while((MAP_I2CMasterIntStatusEx(I2C_BASE, false) ) ){}
    //
    // Check for any errors in transfer
    //
    if(MAP_I2CMasterErr(I2C_BASE) != I2C_MASTER_ERR_NONE)
    {
        switch(ulCmd)
        {
        case I2C_MASTER_CMD_BURST_SEND_START:
        case I2C_MASTER_CMD_BURST_SEND_CONT:
        case I2C_MASTER_CMD_BURST_SEND_STOP:
            MAP_I2CMasterControl(I2C_BASE,
                         I2C_MASTER_CMD_BURST_SEND_ERROR_STOP);
            break;
        case I2C_MASTER_CMD_BURST_RECEIVE_START:
        case I2C_MASTER_CMD_BURST_RECEIVE_CONT:
        case I2C_MASTER_CMD_BURST_RECEIVE_FINISH:
            MAP_I2CMasterControl(I2C_BASE,
                         I2C_MASTER_CMD_BURST_RECEIVE_ERROR_STOP);
            break;
        default:
            break;
        }
        return FAILURE;
    }

    return SUCCESS;
}

//****************************************************************************
//
//! Invokes the I2C driver APIs to write to the specifed address
//!
//! \param ucDevAddr is the device I2C slave address
//! \param pucData is the pointer to the data to be written
//! \param ucLen is the length of data to be written
//! \param ucStop determines if the transaction is followed by stop bit
//! 
//! This function works in a polling mode,
//!    1. Writes the decive register address to be written to.
//!    2. In a loop, writes all the bytes over I2C
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
int 
I2CWrite(unsigned char ucDevAddr,
         unsigned char *pucData,
         unsigned char ucLen, 
         unsigned char ucStop)
{
    RETERR_IF_TRUE(pucData == NULL);
    RETERR_IF_TRUE(ucLen == 0);
    //
    // Set I2C codec slave address
    //
    MAP_I2CMasterSlaveAddrSet(I2C_BASE, ucDevAddr, false);
    //
    // Write the first byte to the controller.
    //
    MAP_I2CMasterDataPut(I2C_BASE, *pucData);
    //
    // Initiate the transfer.
    //
    RET_IF_ERR(I2CTransact(I2C_MASTER_CMD_BURST_SEND_START));
    vTaskDelay(0);
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
    	vTaskDelay(0);
        MAP_I2CMasterDataPut(I2C_BASE, *pucData);
        vTaskDelay(0);
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
    vTaskDelay(0);

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
//! \param ucDevAddr is the device I2C slave address
//! \param pucData is the pointer to the read data to be placed
//! \param ucLen is the length of data to be read
//! 
//! This function works in a polling mode,
//!    1. Writes the decive register address to be written to.
//!    2. In a loop, reads all the bytes over I2C
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
int 
I2CRead(unsigned char ucDevAddr,
        unsigned char *pucData,
        unsigned char ucLen)
{
    unsigned long ulCmdID;

    RETERR_IF_TRUE(pucData == NULL);
    RETERR_IF_TRUE(ucLen == 0);
    //
    // Set I2C codec slave address
    //
    MAP_I2CMasterSlaveAddrSet(I2C_BASE, ucDevAddr, true);

  //  vTaskDelay(0);

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
        *pucData = MAP_I2CMasterDataGet(I2C_BASE);
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

  //  vTaskDelay(0);
	*pucData = MAP_I2CMasterDataGet(I2C_BASE);

    return SUCCESS;
}

//****************************************************************************
//
//! Invokes the I2C driver APIs to read from a specified address the device. 
//! This assumes the device local address to be of 8-bit. For other 
//! combinations use I2CWrite followed by I2CRead.
//!
//! \param ucDevAddr is the device I2C slave address
//! \param pucWrDataBuf is the pointer to the data to be written (reg addr)
//! \param ucWrLen is the length of data to be written
//! \param pucRdDataBuf is the pointer to the read data to be placed
//! \param ucRdLen is the length of data to be read
//! 
//! This function works in a polling mode,
//!    1. Writes the data over I2C (decive register address to be read from).
//!    2. In a loop, reads all the bytes over I2C
//!
//! \return 0: Success, < 0: Failure.
//
//****************************************************************************
int 
I2CReadFrom(unsigned char ucDevAddr,
            unsigned char *pucWrDataBuf,
            unsigned char ucWrLen,
            unsigned char *pucRdDataBuf,
            unsigned char ucRdLen)
{
    //
    // Write the register address to be read from.
    // Stop bit implicitly assumed to be 0.
    //
    RET_IF_ERR(I2CWrite(ucDevAddr,pucWrDataBuf,ucWrLen,0));
    //
    // Read the specified length of data
    //
    RET_IF_ERR(I2CRead(ucDevAddr, pucRdDataBuf, ucRdLen));

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
I2COpen(unsigned long ulMode)
{
    //
    // Enable I2C Peripheral
    //           
	//MAP_HwSemaphoreLock(HWSEM_I2C, HWSEM_WAIT_FOR_EVER);
    MAP_PRCMPeripheralClkEnable(PRCM_I2CA0, PRCM_RUN_MODE_CLK);
    MAP_PRCMPeripheralReset(PRCM_I2CA0);

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
    // Disables the multimaster mode
    //
    //MAP_I2CMasterDisable(I2C_BASE);
    
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
I2CClose()
{
    //
    // Power OFF the I2C peripheral
    //
    MAP_PRCMPeripheralClkDisable(PRCM_I2CA0, PRCM_RUN_MODE_CLK);

    return SUCCESS;
}
