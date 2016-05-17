#ifndef _AUDIOHELPER_H_
#define _AUDIOHELPER_H_

#include "ff.h"
#include <stdint.h>

typedef struct {
	const char * file_name;
	FIL file_obj;
} Filedata_t;

void InitAudioHelper();

uint8_t InitAudioCapture(uint32_t rate);
void DeinitAudioCapture(void);

uint8_t InitAudioPlayback(int32_t vol, uint32_t rate);
void DeinitAudioPlayback(void);


uint8_t InitFile(Filedata_t * pfiledata);
uint8_t WriteToFile(Filedata_t * pfiledata,const UINT bytes_to_write,const uint8_t * const_ptr_samples_bytes);
void CloseFile(Filedata_t * pfiledata);
void CloseAndDeleteFile(Filedata_t * pfiledata);

int deleteFilesInDir(const char* dir);



#endif