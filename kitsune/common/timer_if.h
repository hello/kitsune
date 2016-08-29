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

#ifndef __TIMER_IF_H__
#define __TIMER_IF_H__

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
#define SYS_CLK				    80000000
#define MILLISECONDS_TO_TICKS(ms)   ((SYS_CLK/1000) * (ms))
#define PERIODIC_TEST_LOOPS     5

typedef struct e_Timers
{
	unsigned long	ePeripheral;
	unsigned long	base;
	unsigned long	timer;
}e_Timers;

extern void Timer_IF_Init( unsigned long ePeripheralc, unsigned long ulBase,
    unsigned long ulConfig, unsigned long ulTimer, unsigned long ulValue);
extern void Timer_IF_IntSetup(unsigned long ulBase, unsigned long ulTimer, 
                   void (*TimerBaseIntHandler)(void));
extern void Timer_IF_InterruptClear(unsigned long ulBase);
extern void Timer_IF_Start(unsigned long ulBase, unsigned long ulTimer, 
                unsigned long ulValue);
extern void Timer_IF_Stop(unsigned long ulBase, unsigned long ulTimer);
extern void Timer_IF_ReLoad(unsigned long ulBase, unsigned long ulTimer, 
                unsigned long ulValue);
extern unsigned int Timer_IF_GetCount(unsigned long ulBase, unsigned long ulTimer);
void Timer_IF_DeInit(unsigned long ulBase,unsigned long ulTimer);
void simplelink_timerA2_start();
unsigned long TimerGetCurrentTimestamp();

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif
#endif //  __TIMER_IF_H__
