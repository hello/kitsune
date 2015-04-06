#ifndef __FAULT_H___
#define __FAULT_H___

//top of sram
#define SHUTDOWN_MEM ((void*)((0x20040000-sizeof(faultInfo))))
#define SHUTDOWN_MAGIC 0xDEAD0BAD
#define TRACE_DONE 0xBEEFCAFE
typedef struct {

    unsigned long magic;
    unsigned long version;
    unsigned long faultStatus;
    unsigned long busFaultAddr;
    unsigned long mmuAddr;
    unsigned long exceptionFrame[8];
    unsigned long stack_trace[64];
/* only whole words here ! */
} faultInfo;

void faultPrinter( faultInfo* f );
extern void FaultISR(void);

#endif
