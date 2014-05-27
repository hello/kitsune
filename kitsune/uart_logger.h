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
// uart_logger.h - Prototypes and Macros for UARTLogger
//
//*****************************************************************************
#ifndef __UART_LOGGER_H__
#define __UART_LOGGER_H__

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

/****************************************************************************/
/*								MACROS										*/
/****************************************************************************/
#define UART_BAUD_RATE  115200
#define SYSCLK          80000000
#define CONSOLE         UARTA0_BASE
#define CONSOLE_PERIPH  PRCM_UARTA0

/****************************************************************************/
/*								FUNCTION PROTOTYPES							*/
/****************************************************************************/
extern void InitTerm(void);
extern void ClearTerm(void);
extern void Message(char *format);
extern void Report(char *format, ...);

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif

