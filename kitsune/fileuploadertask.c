#ifdef KIT_INCLUDE_FILE_UPLOAD

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "uartstdio.h"

#include "fileuploadertask.h"
#include "networktask.h"

#include "hellofilesystem.h"
#include "protobuf/filetransfer.pb.h"
#include "debugutils/matmessageutils.h"
#include "sys_time.h"
#include "wifi_cmd.h"
#include "proto_utils.h"

#include <stdbool.h>
#include <string.h>

#define INBOX_QUEUE_LENGTH (6)
#define RETRY_TIMEOUT_TICKS (4000)

//task message type
typedef enum {
	eUpload,
} EUploaderMessage_t;


typedef struct {
	char sfilepath[32]; //need buffer somewhere, as filenames are sometimes not const static
	char * host;
	const char * endpoint;
	uint8_t deleteAfterUpload;

} FileUploaderMessage_t;

//task message
typedef struct {
	EUploaderMessage_t type;

	FileUploadNotification_t onFinished;
	void * context;

	union {
		FileUploaderMessage_t uploadermessage;
	} message;

} TaskMessage_t;

//main context data / encode data
typedef struct {
	uint8_t deleteAfterUpload;
	const char * filename;
	uint32_t unix_time;
} EncodeData_t;

static xQueueHandle _queue = NULL;
static xSemaphoreHandle _wait;

static void Init() {
	if (!_queue) {
		_queue = xQueueCreate(INBOX_QUEUE_LENGTH,sizeof( TaskMessage_t ) );
	}

	if (!_wait) {
		_wait = xSemaphoreCreateBinary();
	}
}

static void NetTaskResponse(const NetworkResponse_t * response,
		char * reply_buf, int reply_sz, void * context) {
	EncodeData_t * data = (EncodeData_t *)context;
	TaskMessage_t m;

	memset(&m,0,sizeof(m));

	//delete even if upload was not successful
	if (data->deleteAfterUpload) {
		FRESULT res;

		res = hello_fs_unlink(data->filename);

		if (res != FR_OK) {
			LOGI("failed to delete file %s\r\n",data->filename);
		}
	}


	xSemaphoreGive(_wait);
}

bool encode_file (pb_ostream_t * stream, const pb_field_t * field,void * const *arg) {
	FRESULT res;
	FIL file_obj;
	FILINFO file_info;

	uint8_t readbuf[512];

	DWORD bytes_to_be_read;
	UINT read_size;
	UINT bytes_read;

	bool success = false;

	const EncodeData_t * encodedata = (const EncodeData_t * )(*arg);


	memset(&file_info,0,sizeof(file_info));
	memset(&file_obj,0,sizeof(file_obj));
	memset(&res,0,sizeof(res));

	//write tag
	if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)) {
		return false;
	}


	/* open file */
	res = hello_fs_open(&file_obj, encodedata->filename, FA_READ);

	/*  Did something horrible happen?  */
	if(res != FR_OK){
		LOGI("File open %s failed: %d\n", encodedata->filename, res);
	}
	else {
		//find out size of file
		hello_fs_stat(encodedata->filename, &file_info);

		bytes_to_be_read = file_info.fsize;

		//write size
		if (!pb_encode_varint(stream, (uint64_t)bytes_to_be_read)) {
			goto FILEUPLOADER_CLEANUP;
		}

		//loop until read, encoding as you go.
		while (bytes_to_be_read > 0) {

			if (bytes_to_be_read > sizeof(readbuf)) {
				read_size = sizeof(readbuf);
			}
			else {
				read_size = bytes_to_be_read;
			}

			res = hello_fs_read(&file_obj,readbuf,read_size,&bytes_read);

			if (res != FR_OK) {
				goto FILEUPLOADER_CLEANUP;
			}

			if (!pb_write(stream, readbuf, bytes_read)) {
				goto FILEUPLOADER_CLEANUP;
			}

			bytes_to_be_read -= bytes_read;
		}

		success = true;

		FILEUPLOADER_CLEANUP:

		hello_fs_close(&file_obj);
	}

	return success;
}


void FileUploaderTask_Thread(void * data) {
	TaskMessage_t m;
	FileUploaderMessage_t * p;
	NetworkTaskServerSendMessage_t mnet;
	EncodeData_t encode_data;
	FileMessage pbfile;

	Init();

	for (; ; ) {
		if (xQueueReceive( _queue,(void *) &m, portMAX_DELAY )) {

			switch (m.type) {

			case eUpload:
			{
				//upload file
				p = &m.message.uploadermessage;
				memset(&mnet,0,sizeof(mnet));

				encode_data.filename = m.message.uploadermessage.sfilepath;
				encode_data.deleteAfterUpload = m.message.uploadermessage.deleteAfterUpload;
				encode_data.unix_time = get_time();

				mnet.endpoint = p->endpoint;
				mnet.host = p->host;
				mnet.response_callback = NetTaskResponse;

				memset(&pbfile,0,sizeof(pbfile));
				pbfile.device_id.funcs.encode = encode_device_id_string;
				pbfile.unix_time = get_time();
				pbfile.has_unix_time = true;
				pbfile.data.funcs.encode = encode_file;
				pbfile.data.arg = &encode_data;

				mnet.structdata = &pbfile;
				mnet.fields = FileMessage_fields;

				mnet.retry_timeout = RETRY_TIMEOUT_TICKS;
				mnet.context = &encode_data;

				//the network task will eventually call our encode callback
				//and send the protobuf onwards to the server
				if (NetworkTask_AddMessageToQueue(&mnet)) {
					//wait until callback, this way we can refer to items on stack (like a filename)
					//and have a guarantee that it won't change out from under us
					xSemaphoreTake(_wait,portMAX_DELAY);
				}

				break;
			}
			}
		}
	}
}

void FileUploaderTask_Upload(const char * filepath,const char * host, const char * endpoint,uint8_t deleteAfterUpload,FileUploadNotification_t onUploaded, void * context) {
	TaskMessage_t m;
	FileUploaderMessage_t * p = &m.message.uploadermessage;

	memset(&m,0,sizeof(m));

	strncpy(p->sfilepath,filepath,sizeof(p->sfilepath));
	p->host = host;
	p->endpoint = endpoint;
	p->deleteAfterUpload = deleteAfterUpload;

	m.onFinished = onUploaded;
	m.context = context;
	m.type = eUpload;


	if (_queue) {
		xQueueSend(_queue,&m,0);
	}
}

#endif
