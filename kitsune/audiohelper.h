#ifndef _AUDIOHELPER_H_
#define _AUDIOHELPER_H_

#include "ff.h"
#include <stdint.h>

uint8_t InitAudioCapture(uint32_t rate);
void DeinitAudioCapture(void);

uint8_t InitAudioPlayback(int32_t vol, uint32_t rate);
void DeinitAudioPlayback(void);

int deleteFilesInDir(const char* dir);



#endif
