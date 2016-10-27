#ifndef _FAKE_SEMPHR_H_
#define _FAKE_SEMPHR_H_

typedef int xSemaphoreHandle; 

static inline int xSemaphoreTakeRecursive(int a, int b) {
    return 1;
}

static inline int xSemaphoreGive(int a) {
    return 1;
}

static inline int xSemaphoreCreateRecursiveMutex() {
    return 1;
}


static inline void  vSemaphoreDelete(int a) {
    
}

                


#endif //_FAKE_SEMPHR_H_
