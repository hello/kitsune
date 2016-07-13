/*
 *   Copyright (C) 2015 Texas Instruments Incorporated
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
 */

//*****************************************************************************
// uart_if.c
//
// uart interface file: Prototypes and Macros for UARTLogger
//
//*****************************************************************************

// Standard includes
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Driverlib includes
#include "hw_types.h"
#include "hw_memmap.h"
#include <driverlib/prcm.h>
#include <driverlib/pin.h>
#include <driverlib/uart.h>
#include <driverlib/rom.h>
#include <driverlib/rom_map.h>

#include <ti/drivers/UART.h>
#include "board.h"

#if defined(USE_FREERTOS) || defined(USE_TI_RTOS)
#include "osi.h"
#endif

#include "uart_if.h"

#define IS_SPACE(x)       (x == 32 ? 1 : 0)

//*****************************************************************************
// Global variable indicating command is present
//*****************************************************************************
static unsigned long    __Errorlog;
static UART_Handle      uartHandle;

//*****************************************************************************
// Global variable indicating input length
//*****************************************************************************
unsigned int ilen=1;


//*****************************************************************************
//
//! Initialization
//!
//! This function
//!        1. Configures the UART to be used.
//!
//! \return none
//
//*****************************************************************************
UART_Handle
InitTerm()
{
#ifndef NOTERM
    UART_Params         uartParams;
      
    Board_initUART();
    UART_Params_init(&uartParams);
    uartParams.writeDataMode = UART_DATA_BINARY;
    uartParams.readDataMode = UART_DATA_BINARY;
    uartParams.readReturnMode = UART_RETURN_FULL;
    uartParams.readEcho = UART_ECHO_OFF;
    uartParams.baudRate = 115200;
    uartHandle = UART_open(Board_UART0, &uartParams);
    
#endif
    __Errorlog = 0;
    return(uartHandle);

}

//*****************************************************************************
//
//!    Outputs a character string to the console
//!
//! \param str is the pointer to the string to be printed
//!
//! This function
//!        1. prints the input string character by character on to the console.
//!
//! \return none
//
//  \note If UART_NONPOLLING defined in than Message or UART write should be
//			called in task/thread context only.
//*****************************************************************************
void 
Message(const char *str)
{
#ifndef NOTERM
#ifdef UART_NONPOLLING
	UART_write(uartHandle, str, strlen(str));
#else
	UART_writePolling(uartHandle, str, strlen(str));
#endif
#endif
}

//*****************************************************************************
//
//!    Outputs a character to the console
//!
//! \param char	- A character to be printed
//!
//! \return none
//
//*****************************************************************************
void
putch(char ch)
{
  UART_writePolling(uartHandle, &ch, 1);
}

//*****************************************************************************
//
//!    Read a character from the console
//!
//! \param none
//!
//! \return Character
//
//*****************************************************************************
char 
getch(void)
{
  char ch;
  
  UART_readPolling(uartHandle, &ch, 1);
  return ch;
}

//*****************************************************************************
//
//!    Clear the console window
//!
//! This function
//!        1. clears the console window.
//!
//! \return none
//
//*****************************************************************************
void 
ClearTerm()
{
    Message("\33[2J\r");
}

//*****************************************************************************
//
//! Error Function
//!
//! \param 
//!
//! \return none
//! 
//*****************************************************************************
void 
Error(char *pcFormat, ...)
{
#ifndef NOTERM
    char  cBuf[256];
    va_list list;
    va_start(list,pcFormat);
    vsnprintf(cBuf,256,pcFormat,list);
    Message(cBuf);
#endif
    __Errorlog++;
}

//*****************************************************************************
//
//! Get the Command string from UART
//!
//! \param  pucBuffer is the command store to which command will be populated
//! \param  ucBufLen is the length of buffer store available
//!
//! \return Length of the bytes received. -1 if buffer length exceeded.
//! 
//*****************************************************************************
int
GetCmd(char *pcBuffer, unsigned int uiBufLen)
{
    char cChar;
    int iLen = 0;
    
    UART_readPolling(uartHandle, &cChar, 1);
        
    iLen = 0;
    
    //
    // Checking the end of Command
    //
    while(1)
    {
        //
        // Handling overflow of buffer
        //
        if(iLen >= uiBufLen)
        {
            return -1;
        }
        
        //
        // Copying Data from UART into a buffer
        //
        if((cChar == '\r') || (cChar =='\n'))
        {
            UART_writePolling(uartHandle, &cChar, 1);
            break;
        }
        else if(cChar == '\b')
        {
        	//
        	// Deleting last character when you hit backspace
        	//
                char    ch;
                
    		UART_writePolling(uartHandle, &cChar, 1);
                ch = ' ';
    		UART_writePolling(uartHandle, &ch, 1);
        	if(iLen)
        	{
        		UART_writePolling(uartHandle, &cChar, 1);
        		iLen--;
        	}
        	else
        	{
                    ch = '\a';
        		UART_writePolling(uartHandle, &ch, 1);
        	}
        }
        else
        { 
        	//
        	// Echo the received character
        	//
        	UART_writePolling(uartHandle, &cChar, 1);

        	*(pcBuffer + iLen) = cChar;
        	iLen++;
        }
        
        UART_readPolling(uartHandle, &cChar, 1);
        
    }

    *(pcBuffer + iLen) = '\0';

    return iLen;
}

//*****************************************************************************
//
//!    Trim the spaces from left and right end of given string
//!
//! \param  Input string on which trimming happens
//!
//! \return length of trimmed string
//
//*****************************************************************************
int TrimSpace(char * pcInput)
{
    size_t size;
    char *endStr, *strData = pcInput;
    char index = 0;
    size = strlen(strData);

    if (!size)
        return 0;

    endStr = strData + size - 1;
    while (endStr >= strData && IS_SPACE(*endStr))
        endStr--;
    *(endStr + 1) = '\0';

    while (*strData && IS_SPACE(*strData))
    {
        strData++;
        index++;
    }
    memmove(pcInput,strData,strlen(strData)+1);

    return strlen(pcInput);
}

//*****************************************************************************
//
//!    prints the formatted string on to the console
//!
//! \param format is a pointer to the character string specifying the format in
//!           the following arguments need to be interpreted.
//! \param [variable number of] arguments according to the format in the first
//!         parameters
//! This function
//!        1. prints the formatted error statement.
//!
//! \return count of characters printed
//
//*****************************************************************************
int Report(const char *pcFormat, ...)
{
 int iRet = 0;
#ifndef NOTERM

  char *pcBuff, *pcTemp;
  int iSize = 256;
 
  va_list list;
  pcBuff = (char*)malloc(iSize);
  if(pcBuff == NULL)
  {
      return -1;
  }
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
              iRet = -1;
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
  return iRet;
}


