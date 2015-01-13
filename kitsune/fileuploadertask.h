#ifndef _FILEUPLOADERTASK_H_
#define _FILEUPLOADERTASK_H_

#include <stdint.h>

typedef void (*FileUploadNotification_t)(void *);

#define UPLOADER_GROUP_ID_AUDIO_TASK (123456)

void FileUploaderTask_Thread(void * data);

uint8_t FileUploaderTask_Upload(const char * filepath,const char * host, const char * endpoint,uint8_t deleteAfterUpload,
		                     uint32_t group_id,int32_t delayInTicks,FileUploadNotification_t onUploaded, void * context,uint32_t maxWaitIfBusy);

void FileUploaderTask_CancelUploadByGroup(uint32_t group);

#endif
