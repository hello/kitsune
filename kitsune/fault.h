#ifndef __FAULT_H___
#define __FAULT_H___

//top of sram
#define SHUTDOWN_MEM ((void*)((0x20040000-100)))
#define SHUTDOWN_MAGIC 0xDEAD0BAD
#define TRACE_DONE 0xBEEFCAFE
#define MAX_TRACE_DEPTH 256
typedef struct {

    unsigned long magic;
    unsigned long version;
    unsigned long faultStatus;
    unsigned long busFaultAddr;
    unsigned long mmuAddr;
    unsigned long exceptionFrame[8];
//    unsigned long stack_trace[MAX_TRACE_DEPTH];
/* only whole words here ! */
} faultInfo;

void faultPrinter( faultInfo* f );
void FaultISR(void);

#endif
