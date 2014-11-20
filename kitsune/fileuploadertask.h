#ifndef _FILEUPLOADERTASK_H_
#define _FILEUPLOADERTASK_H_

#include <stdint.h>

typedef void (*FileUploadNotification_t)(void *);



void FileUploaderTask_Thread(void * data);

void FileUploaderTask_Upload(const char * filepath,const char * host, const char * endpoint,uint8_t deleteAfterUpload,FileUploadNotification_t onUploaded, void * context);

#endif
