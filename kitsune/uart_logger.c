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
//*****************************************************************************
//
//! \addtogroup freertos_demo
//! @{
//
//*****************************************************************************
//*****************************************************************************
//
// uart_logger.c - API(s) for UARTLogger
//
//*****************************************************************************
#include <hw_types.h>
#include <hw_memmap.h>
#include <prcm.h>
#include <pin.h>
#include <uart.h>

#include <stdarg.h>
#include<stdlib.h>
#include <stdio.h>
#include "rom_map.h"
#include <uart_logger.h>

//*****************************************************************************
//
//! Initialisation
//!
//! This function
//!		1. Configures the UART to be used.
//!
//! \return none
//
//*****************************************************************************
void InitTerm()
{
    MAP_PRCMPeripheralClkEnable(CONSOLE_PERIPH, PRCM_RUN_MODE_CLK);
	MAP_UARTConfigSetExpClk(CONSOLE,MAP_PRCMPeripheralClockGet(CONSOLE_PERIPH), UART_BAUD_RATE,
		                            (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
		                             UART_CONFIG_PAR_NONE));
}

//*****************************************************************************
//
//!	Outputs a character string to the console
//!
//! \param str is the pointer to the string to be printed
//!
//! This function
//!		1. prints the input string character by character on to the console.
//!
//! \return none
//
//*****************************************************************************
void Message(char *str)
{
#ifndef NOTERM
  if(str != NULL)
  {
    while(*str!='\0')
    {
        MAP_UARTCharPut(CONSOLE,*str++);
    }
  }
#endif
}

//*****************************************************************************
//
//!	Clear the console window
//!
//! This function
//!		1. clears the console window.
//!
//! \return none
//
//*****************************************************************************
void ClearTerm()
{
   Message("\33[2J\r");
}

//*****************************************************************************
//
//!	prints the formatted string on to the console
//!
//! \param format is a pointer to the character string specifying the format in
//!		   the following arguments need to be interpreted.
//! \param [variable number of] arguments according to the format in the first
//!         parameters
//! This function
//!		1. prints the formatted error statement.
//!
//! \return none
//
//*****************************************************************************
void Report(char *pcFormat, ...)
{

#ifndef NOTERM

    char *pcBuff, *pcTemp;
    int iSize = 256;
    int iRet;
    pcBuff = (char*)malloc(iSize);
    va_list list;
    while(1)
    {
        va_start(list,pcFormat);
        iRet = vsnprintf(pcBuff,iSize,pcFormat,list);
        va_end(list);
        if(iRet > -1 && iRet < iSize)
        {
            break;
        }
        else
        {
            iSize*=2;
            if((pcTemp=realloc(pcBuff,iSize))==NULL)
            { 
                Message("Could not reallocate memory\n\r");
                break;
            }
            else
            {
               pcBuff=pcTemp;
            }
              
       }
    }
    Message(pcBuff);
    free(pcBuff);

#endif

}

//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************
