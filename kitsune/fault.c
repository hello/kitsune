//*****************************************************************************
//
// Fault handlers
//
//*****************************************************************************

#include "hw_nvic.h"
#include "hw_types.h"

#include "uartstdio.h"
#include "fault.h"

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
void faultPrinter( faultInfo* f ) {
	int i;
    //
    // Check for any bits set in the usage fault field.  Print a human
    // readable string for any bits that are set.
    //
    if(f->faultStatus & 0xffff0000)
    {
        UARTprintf("\nUSAGE FAULT:\n");
        for(i = 0; i < 6; i++)
        {
            if(f->faultStatus & sUsageFaultMap[i].ulFaultBits)
            {
                UARTprintf(" %s\n", sUsageFaultMap[i].cFaultText);
            }
        }
    }

    //
    // Check for any bits set in the bus fault field.  Print a human
    // readable string for any bits that are set.
    //
    if(f->faultStatus & 0x0000ff00)
    {
        UARTprintf("\nBUS FAULT:\n");
        for(i = 0; i < 5; i++)
        {
            if(f->faultStatus & sBusFaultMap[i].ulFaultBits)
            {
                UARTprintf(" %s\n", sBusFaultMap[i].cFaultText);
            }
        }

        //
        // Also print the faulting address if it is available.
        //
        if(f->faultStatus & NVIC_FAULT_STAT_BFARV)
        {
            UARTprintf("BFAR = %08X\n", f->busFaultAddr);
        }
    }

    //
    // Check for any bits set in the memory fault field.  Print a human
    // readable string for any bits that are set.
    //
    if(f->faultStatus & 0x000000ff)
    {
        UARTprintf("\nMEMORY MANAGE FAULT:\n");
        for(i = 0; i < 4; i++)
        {
            if(f->faultStatus & sMemFaultMap[i].ulFaultBits)
            {
                UARTprintf(" %s\n", sMemFaultMap[i].cFaultText);
            }
        }

        //
        // Also print the faulting address if it is available.
        //
        if(f->faultStatus & NVIC_FAULT_STAT_MMARV)
        {
            UARTprintf("MMAR = %08X\n", f->mmuAddr);
        }
    }

    //
    // Print the context of the exception stack frame.
    //
    UARTprintf("\nException Frame\n---------------\n");
    UARTprintf("R0   = %08X\n", f->exceptionFrame[0]);
    UARTprintf("R1   = %08X\n", f->exceptionFrame[1]);
    UARTprintf("R2   = %08X\n", f->exceptionFrame[2]);
    UARTprintf("R3   = %08X\n", f->exceptionFrame[3]);
    UARTprintf("R12  = %08X\n", f->exceptionFrame[4]);
    UARTprintf("LR   = %08X\n", f->exceptionFrame[5]);
    UARTprintf("PC   = %08X\n", f->exceptionFrame[6]);
    UARTprintf("xPSR = %08X\n", f->exceptionFrame[7]);

}

void
FaultDecoder(unsigned long *pulExceptionFrame)
{
    unsigned int i;

    static faultInfo f;

    //
    // Read the fault status register.
    //
    f.faultStatus = HWREG(NVIC_FAULT_STAT);

    //
    // Check for any bits set in the bus fault field.  Print a human
    // readable string for any bits that are set.
    //
    if(f.faultStatus & 0x0000ff00)
    {
        if(f.faultStatus & NVIC_FAULT_STAT_BFARV)
        {
        	f.busFaultAddr = HWREG(NVIC_FAULT_ADDR);
        }
    }

    //
    // Check for any bits set in the memory fault field.  Print a human
    // readable string for any bits that are set.
    //
    if(f.faultStatus & 0x000000ff)
    {
        if(f.faultStatus & NVIC_FAULT_STAT_MMARV)
        {
        	f.mmuAddr = HWREG(NVIC_MM_ADDR);
        }
    }

    for( i=0;i<8;++i )
    	f.exceptionFrame[i] = pulExceptionFrame[i];

    f.magic = SHUTDOWN_MAGIC;

    //todo flash it FlashProgram((void*)&f, SHUTDOWN_MEM, sizeof(f));
    //till then just print it out.
    faultPrinter(&f);

    while(1) {UARTprintf("");};
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


