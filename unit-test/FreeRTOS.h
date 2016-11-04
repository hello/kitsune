#ifndef _FREERTOS_H_
#define _FREERTOS_H_

#include <stdlib.h>

#define pvPortMalloc malloc

static inline void vPortFree(void * stream) {
    free(stream);
}

#define portMAX_DELAY (0x7FFFFFFF)
#endif //_FREERTOS_H_
