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
// uart_if.h
//
// uart interface header file: Prototypes and Macros for UARTLogger
//
//*****************************************************************************

#ifndef __UART_IF_H__
#define __UART_IF_H__

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif


#include "UART.h"

/****************************************************************************/
/*								MACROS										*/
/****************************************************************************/
#define UART_BAUD_RATE  115200
#define SYSCLK          80000000
#define CONSOLE         UARTA0_BASE
#define CONSOLE_PERIPH  PRCM_UARTA0
//
// Define the size of UART IF buffer for RX
//
#define UART_IF_BUFFER           64

//
// Define the UART IF buffer
//
extern unsigned char g_ucUARTBuffer[];


/****************************************************************************/
/*								FUNCTION PROTOTYPES							*/
/****************************************************************************/
extern void DispatcherUARTConfigure(void);
extern void DispatcherUartSendPacket(unsigned char *inBuff, unsigned short usLength);
extern int GetCmd(char *pcBuffer, unsigned int uiBufLen);
extern UART_Handle InitTerm(void);
extern void ClearTerm(void);
extern void Message(const char *format);
extern void Error(char *format,...);
extern int TrimSpace(char * pcInput);
//extern int Report(const char *format, ...);
extern void putch(char ch);
extern char getch(void);

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif

