#ifndef _TINYTENSOR_MEMORY_H_
#define _TINYTENSOR_MEMORY_H_

#ifdef __cplusplus
extern "C" {
#endif

    
#ifdef DESKTOP
#include <memory.h>
#include <string.h>
#include <stdlib.h>
#define MALLLOC(x) malloc(x)
#define FREE(x) free(x)
#define MEMCPY(tgt,src,size) memcpy(tgt,src,size)
#define MEMSET(tgt,val,size) memset(tgt,val,size)
#else
#include "FreeRTOS.h"
#include <string.h>

#define MALLLOC(x) pvPortMalloc(x)
#define FREE(x) pvPortFree(x)
#define MEMCPY(tgt,src,size) memcpy(tgt,src,size)
#define MEMSET(tgt,val,size) memset(tgt,val,size)

#endif
    
#ifdef __cplusplus
}
#endif

#endif //_TINYTENSOR_MEMORY_H_
