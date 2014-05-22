//*****************************************************************************
//
// startup_ccs.c - Startup code for use with TI's Code Composer Studio.
//
// Copyright (c) 2006-2010 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
//
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
//
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
//
// Modified for EK-LM4F232 Eval board on 5/18/2012 by:
//
//     Ken Pettit
//     Fuel7, Inc.
//
//*****************************************************************************

#include "inc/hw_nvic.h"
#include "inc/hw_types.h"
#include "uartstdio.h"


//*****************************************************************************
//
// Forward declaration of the default fault handlers.
//
//*****************************************************************************
void ResetISR(void);
static void NmiSR(void);
static void FaultISR(void);
static void IntDefaultHandler(void);

//*****************************************************************************
//
// External declaration for the reset handler that is to be called when the
// processor is started
//
//*****************************************************************************
extern void _c_int00(void);

//*****************************************************************************
//
// Linker variable that marks the top of the stack.
//
//*****************************************************************************
extern unsigned long __STACK_TOP;

//*****************************************************************************
//
// External declarations for the interrupt handlers used by the application.
//
//*****************************************************************************
extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);
extern void vPortSVCHandler( void );

//*****************************************************************************
//
// The vector table.  Note that the proper constructs must be placed on this to
// ensure that it ends up at physical address 0x0000.0000 or at the start of
// the program if located at a start address other than 0.
//
//*****************************************************************************
#pragma DATA_SECTION(g_pfnVectors, ".intvecs")
void (* const g_pfnVectors[])(void) =
{
    (void (*)(void))((unsigned long)&__STACK_TOP),
                                            // The initial stack pointer
    ResetISR,                               // The reset handler
    NmiSR,                                  // The NMI handler
    FaultISR,                               // The hard fault handler
    IntDefaultHandler,                      // The MPU fault handler
    IntDefaultHandler,                      // The bus fault handler
    IntDefaultHandler,                      // The usage fault handler
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    vPortSVCHandler,                        // SVCall handler (System Service Call with SVC instruction)
    IntDefaultHandler,                      // Debug monitor handler
    0,                                      // Reserved
    xPortPendSVHandler,                     // The PendSV handler (Pendable request for system service)
    xPortSysTickHandler,                    // The SysTick handler
    IntDefaultHandler,                      // GPIO Port A
    IntDefaultHandler,                      // GPIO Port B
    IntDefaultHandler,                      // GPIO Port C
    IntDefaultHandler,                      // GPIO Port D
    IntDefaultHandler,                      // GPIO Port E
    IntDefaultHandler,                      // UART0 Rx and Tx
    IntDefaultHandler,                      // UART1 Rx and Tx
    IntDefaultHandler,                      // SSI0 Rx and Tx
    IntDefaultHandler,                      // I2C0 Master and Slave
    IntDefaultHandler,                      // PWM Fault
    IntDefaultHandler,                      // PWM Generator 0
    IntDefaultHandler,                      // PWM Generator 1
    IntDefaultHandler,                      // PWM Generator 2
    IntDefaultHandler,                      // Quadrature Encoder 0
    IntDefaultHandler,                      // ADC Sequence 0
    IntDefaultHandler,                      // ADC Sequence 1
    IntDefaultHandler,                      // ADC Sequence 2
    IntDefaultHandler,                      // ADC Sequence 3
    IntDefaultHandler,                      // Watchdog timer
    IntDefaultHandler,                      // Timer 0 subtimer A
    IntDefaultHandler,                      // Timer 0 subtimer B
    IntDefaultHandler,                      // Timer 1 subtimer A
    IntDefaultHandler,                      // Timer 1 subtimer B
    IntDefaultHandler,                      // Timer 2 subtimer A
    IntDefaultHandler,                      // Timer 2 subtimer B
    IntDefaultHandler,                      // Analog Comparator 0
    IntDefaultHandler,                      // Analog Comparator 1
    IntDefaultHandler,                      // Analog Comparator 2
    IntDefaultHandler,                      // System Control (PLL, OSC, BO)
    IntDefaultHandler,                      // FLASH Control
    IntDefaultHandler,                      // GPIO Port F
    IntDefaultHandler,                      // GPIO Port G
    IntDefaultHandler,                      // GPIO Port H
    IntDefaultHandler,                      // UART2 Rx and Tx
    IntDefaultHandler,                      // SSI1 Rx and Tx
    IntDefaultHandler,                      // Timer 3 subtimer A
    IntDefaultHandler,                      // Timer 3 subtimer B
    IntDefaultHandler,                      // I2C1 Master and Slave
    IntDefaultHandler,                      // Quadrature Encoder 1
    IntDefaultHandler,                      // CAN0
    IntDefaultHandler,                      // CAN1
    IntDefaultHandler,                      // CAN2
    IntDefaultHandler,                      // Ethernet
    IntDefaultHandler,                      // Hibernate
    IntDefaultHandler,                      // USB0
    IntDefaultHandler,                      // PWM Generator 3
    IntDefaultHandler,                      // uDMA Software Transfer
    IntDefaultHandler,                      // uDMA Error
    IntDefaultHandler,                      // ADC1 Sequence 0
    IntDefaultHandler,                      // ADC1 Sequence 1
    IntDefaultHandler,                      // ADC1 Sequence 2
    IntDefaultHandler,                      // ADC1 Sequence 3
    IntDefaultHandler,                      // I2S0
    IntDefaultHandler,                      // External Bus Interface 0
    IntDefaultHandler,                      // GPIO Port J
    IntDefaultHandler,                      // GPIO Port K
    IntDefaultHandler,                      // GPIO Port L
    IntDefaultHandler,                      // SSI2 Rx and Tx
    IntDefaultHandler,                      // SSI3 Rx and Tx
    IntDefaultHandler,                      // UART3 Rx and Tx
    IntDefaultHandler,                      // UART4 Rx and Tx
    IntDefaultHandler,                      // UART5 Rx and Tx
    IntDefaultHandler,                      // UART6 Rx and Tx
    IntDefaultHandler,                      // UART7 Rx and Tx
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    IntDefaultHandler,                      // I2C2 Master and Slave
    IntDefaultHandler,                      // I2C3 Master and Slave
    IntDefaultHandler,                      // Timer 4 subtimer A
    IntDefaultHandler,                      // Timer 4 subtimer B
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    0,                                      // Reserved
    IntDefaultHandler,                      // Timer 5 subtimer A
    IntDefaultHandler,                      // Timer 5 subtimer B
    IntDefaultHandler,                      // Wide Timer 0 subtimer A
    IntDefaultHandler,                      // Wide Timer 0 subtimer B
    IntDefaultHandler,                      // Wide Timer 1 subtimer A
    IntDefaultHandler,                      // Wide Timer 1 subtimer B
    IntDefaultHandler,                      // Wide Timer 2 subtimer A
    IntDefaultHandler,                      // Wide Timer 2 subtimer B
    IntDefaultHandler,                      // Wide Timer 3 subtimer A
    IntDefaultHandler,                      // Wide Timer 3 subtimer B
    IntDefaultHandler,                      // Wide Timer 4 subtimer A
    IntDefaultHandler,                      // Wide Timer 4 subtimer B
    IntDefaultHandler,                      // Wide Timer 5 subtimer A
    IntDefaultHandler,                      // Wide Timer 5 subtimer B
    IntDefaultHandler,                      // FPU
    IntDefaultHandler,                      // PECI 0
    IntDefaultHandler,                      // LPC 0
    IntDefaultHandler,                      // I2C4 Master and Slave
    IntDefaultHandler,                      // I2C5 Master and Slave
    IntDefaultHandler,                      // GPIO Port M
    IntDefaultHandler,                      // GPIO Port N
    IntDefaultHandler,                      // Quadrature Encoder 2
    IntDefaultHandler,                      // Fan 0
    0,                                      // Reserved
    IntDefaultHandler,                      // GPIO Port P (Summary or P0)
    IntDefaultHandler,                      // GPIO Port P1
    IntDefaultHandler,                      // GPIO Port P2
    IntDefaultHandler,                      // GPIO Port P3
    IntDefaultHandler,                      // GPIO Port P4
    IntDefaultHandler,                      // GPIO Port P5
    IntDefaultHandler,                      // GPIO Port P6
    IntDefaultHandler,                      // GPIO Port P7
    IntDefaultHandler,                      // GPIO Port Q (Summary or Q0)
    IntDefaultHandler,                      // GPIO Port Q1
    IntDefaultHandler,                      // GPIO Port Q2
    IntDefaultHandler,                      // GPIO Port Q3
    IntDefaultHandler,                      // GPIO Port Q4
    IntDefaultHandler,                      // GPIO Port Q5
    IntDefaultHandler,                      // GPIO Port Q6
    IntDefaultHandler,                      // GPIO Port Q7
    IntDefaultHandler,                      // GPIO Port R
    IntDefaultHandler,                      // GPIO Port S
    IntDefaultHandler,                      // PWM 1 Generator 0
    IntDefaultHandler,                      // PWM 1 Generator 1
    IntDefaultHandler,                      // PWM 1 Generator 2
    IntDefaultHandler,                      // PWM 1 Generator 3
    IntDefaultHandler                       // PWM 1 Fault
};

//*****************************************************************************
//
// This is the code that gets called when the processor first starts execution
// following a reset event.  Only the absolutely necessary set is performed,
// after which the application supplied entry() routine is called.  Any fancy
// actions (such as making decisions based on the reset cause register, and
// resetting the bits in that register) are left solely in the hands of the
// application.
//
//*****************************************************************************
void
ResetISR(void)
{
    //
    // Jump to the CCS C Initialization Routine.
    //
    __asm("    .global _c_int00\n"
          "    b.w     _c_int00");
}

//*****************************************************************************
//
// This is the code that gets called when the processor receives a NMI.  This
// simply enters an infinite loop, preserving the system state for examination
// by a debugger.
//
//*****************************************************************************
static void
NmiSR(void)
{
    //
    // Enter an infinite loop.
    //
    while(1)
    {
    }
}

//*****************************************************************************
//
// This is the code that gets called when the processor receives a fault
// interrupt.  This simply enters an infinite loop, preserving the system state
// for examination by a debugger.
//
//*****************************************************************************

//*****************************************************************************
//
// A structure to map fault bit IDs with a human readable text string.
//
//*****************************************************************************
typedef struct
{
    unsigned long ulFaultBits;
    char *cFaultText;
}
tFaultMap;

//*****************************************************************************
//
// Text mapping for the usage, bus, and memory faults.
//
//*****************************************************************************
static const tFaultMap sUsageFaultMap[6] =
{
    { NVIC_FAULT_STAT_DIV0,     "Divide by 0"           },
    { NVIC_FAULT_STAT_UNALIGN,  "Unaligned access"      },
    { NVIC_FAULT_STAT_NOCP,     "No coprocessor"        },
    { NVIC_FAULT_STAT_INVPC,    "Invalid PC"            },
    { NVIC_FAULT_STAT_INVSTAT,  "Invalid state (EPSR)"  },
    { NVIC_FAULT_STAT_UNDEF,    "Undefined instruction" }
};
static const tFaultMap sBusFaultMap[5] =
{
    { NVIC_FAULT_STAT_BSTKE,    "Exception stacking bus error"      },
    { NVIC_FAULT_STAT_BUSTKE,   "Exception return unstacking error" },
    { NVIC_FAULT_STAT_IMPRE,    "Imprecise error"                   },
    { NVIC_FAULT_STAT_PRECISE,  "Precise error"                     },
    { NVIC_FAULT_STAT_IBUS,     "Instruction bus error"             }
};
static const tFaultMap sMemFaultMap[4] =
{
    { NVIC_FAULT_STAT_MSTKE,    "Exception stacking memory access error"   },
    { NVIC_FAULT_STAT_MUSTKE,   "Exception return unstacking access error" },
    { NVIC_FAULT_STAT_DERR,     "Data access violation"                    },
    { NVIC_FAULT_STAT_IERR,     "Instruction access violation"             }
};

//*****************************************************************************
//
// Reads the NVIC fault status register and prints human readable strings
// for the bits that are set.  Also dumps the exception frame.
//
//*****************************************************************************
void
FaultDecoder(unsigned long *pulExceptionFrame)
{
    unsigned long ulFaultStatus;
    unsigned int uIdx;

    //
    // Read the fault status register.
    //
    ulFaultStatus = HWREG(NVIC_FAULT_STAT);

    //
    // The uart stdio output may not yet be initted ...
    //
    UARTStdioInit(0);

    //
    // Check for any bits set in the usage fault field.  Print a human
    // readable string for any bits that are set.
    //
    if(ulFaultStatus & 0xffff0000)
    {
        UARTprintf("\nUSAGE FAULT:\n");
        for(uIdx = 0; uIdx < 6; uIdx++)
        {
            if(ulFaultStatus & sUsageFaultMap[uIdx].ulFaultBits)
            {
                UARTprintf(" %s\n", sUsageFaultMap[uIdx].cFaultText);
            }
        }
    }

    //
    // Check for any bits set in the bus fault field.  Print a human
    // readable string for any bits that are set.
    //
    if(ulFaultStatus & 0x0000ff00)
    {
        UARTprintf("\nBUS FAULT:\n");
        for(uIdx = 0; uIdx < 5; uIdx++)
        {
            if(ulFaultStatus & sBusFaultMap[uIdx].ulFaultBits)
            {
                UARTprintf(" %s\n", sBusFaultMap[uIdx].cFaultText);
            }
        }

        //
        // Also print the faulting address if it is available.
        //
        if(ulFaultStatus & NVIC_FAULT_STAT_BFARV)
        {
            UARTprintf("BFAR = %08X\n", HWREG(NVIC_FAULT_ADDR));
        }
    }

    //
    // Check for any bits set in the memory fault field.  Print a human
    // readable string for any bits that are set.
    //
    if(ulFaultStatus & 0x000000ff)
    {
        UARTprintf("\nMEMORY MANAGE FAULT:\n");
        for(uIdx = 0; uIdx < 4; uIdx++)
        {
            if(ulFaultStatus & sMemFaultMap[uIdx].ulFaultBits)
            {
                UARTprintf(" %s\n", sMemFaultMap[uIdx].cFaultText);
            }
        }

        //
        // Also print the faulting address if it is available.
        //
        if(ulFaultStatus & NVIC_FAULT_STAT_MMARV)
        {
            UARTprintf("MMAR = %08X\n", HWREG(NVIC_MM_ADDR));
        }
    }

    //
    // Print the context of the exception stack frame.
    //
    UARTprintf("\nException Frame\n---------------\n");
    UARTprintf("R0   = %08X\n", pulExceptionFrame[0]);
    UARTprintf("R1   = %08X\n", pulExceptionFrame[1]);
    UARTprintf("R2   = %08X\n", pulExceptionFrame[2]);
    UARTprintf("R3   = %08X\n", pulExceptionFrame[3]);
    UARTprintf("R12  = %08X\n", pulExceptionFrame[4]);
    UARTprintf("LR   = %08X\n", pulExceptionFrame[5]);
    UARTprintf("PC   = %08X\n", pulExceptionFrame[6]);
    UARTprintf("xPSR = %08X\n", pulExceptionFrame[7]);

    //
    // Hang forever.
    //
    while(1)
    {
    }
}

//*****************************************************************************
//
// This is the fault handler that will find the start of the exception
// frame and pass it to the fault decoder.
//
//*****************************************************************************
#if defined(gcc) || defined(sourcerygxx) || defined(codered)
void __attribute__((naked))
FaultISR(void)
{
    __asm("    mov     r0, sp\n"
          "    bl      FaultDecoder\n"
          "    bx      lr");
}
#endif
#if defined(ewarm)
void
FaultISR(void)
{
    __asm("    mov     r0, sp\n"
          "    bl      FaultDecoder\n"
          "    bx      lr");
}
#endif
#if defined(rvmdk) || defined (__ARMCC_VERSION)
__asm void
FaultISR(void)
{
    mov     r0, sp
    bl      __cpp(FaultDecoder)
    bx      lr
}
#endif
#if defined(ccs)
void
FaultISR(void)
{
    __asm("loop1?: mov     r0, sp\n"
          "    bl      FaultDecoder\n"
          "    bx      lr");
}
#endif // #if defined (CCS)


//*****************************************************************************
//
// This is the code that gets called when the processor receives an unexpected
// interrupt.  This simply enters an infinite loop, preserving the system state
// for examination by a debugger.
//
//*****************************************************************************
static void
IntDefaultHandler(void)
{
    //
    // Go into an infinite loop.
    //
    while(1)
    {
    }
}

#if (0)
//*****************************************************************************
//
// A dummy printf function to satisfy the calls to printf from uip.  This
// avoids pulling in the run-time library.
//
//*****************************************************************************
int
uipprintf(const char *fmt, ...)
{
    return(0);
}
#endif
