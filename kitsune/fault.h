#ifndef __FAULT_H___
#define __FAULT_H___

#define SHUTDOWN_MEM ((void*)((0x40000 - sizeof( faultInfo ))&~0xFF))
#define SHUTDOWN_MAGIC 0xDEAD0BAD
typedef struct {

	unsigned long magic;
    unsigned long faultStatus;
    unsigned long busFaultAddr;
    unsigned long mmuAddr;
	unsigned long exceptionFrame[8];
/* only whole words here ! */
} faultInfo;

void faultPrinter( faultInfo* f );
void FaultISR(void);

#endif
