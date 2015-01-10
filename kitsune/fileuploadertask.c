#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "uartstdio.h"
#include "task.h"

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
#include "uart_logger.h"

#define RETRY_TIMEOUT_TICKS (4000)

#define MAX_FILEPATH_LEN (32)
#define NUM_UPLOADS_TRACKED (5)

#define POLLING_PERIOD_IN_TICKS (1000)

typedef struct TFileUploadListItem {
	struct TFileUploadListItem * next;
	int32_t delay_in_ticks;
	uint32_t group_id;

	char sfilepath[MAX_FILEPATH_LEN]; //need buffer somewhere, as filenames are sometimes not const static
	const char * host;
	const char * endpoint;
	uint8_t deleteAfterUpload;
	FileUploadNotification_t onFinished;
	void * context;
	uint32_t unix_time_of_send;
} FileUploadListItem_t;

static FileUploadListItem_t * _pListMem;

static FileUploadListItem_t * _pFreeHead;
static FileUploadListItem_t * _pWorkingHead;
static FileUploadListItem_t * _pCountdownHead;

static xSemaphoreHandle _listMutex;
static xSemaphoreHandle _sem;

static void Init() {

	uint16_t i;

	if (!_sem) {
		_sem = xSemaphoreCreateCounting(NUM_UPLOADS_TRACKED,NUM_UPLOADS_TRACKED);
	}

	if (!_listMutex) {
		_listMutex = xSemaphoreCreateMutex();
	}

	if (!_pListMem) {
		_pListMem = pvPortMalloc(NUM_UPLOADS_TRACKED * sizeof(FileUploadListItem_t));
	}

	if (!_pListMem) {
		LOGE("UNABLE TO ALLOCATE SUFFICIENT MEMORY\n");  //and god help you if you try to upload a file
		return;
	}

	memset(_pListMem,0,NUM_UPLOADS_TRACKED * sizeof(FileUploadListItem_t));
	_pFreeHead = _pListMem;
	for (i = 0; i < NUM_UPLOADS_TRACKED - 1; i++) {
		_pFreeHead[i].next = &_pFreeHead[i+1];
	}
}

static uint8_t MoveItemToFront(FileUploadListItem_t ** tolist,FileUploadListItem_t ** fromlist,FileUploadListItem_t * item) {
	FileUploadListItem_t * p = *fromlist;
	FileUploadListItem_t * pprev = NULL;
	uint8_t success = 0;

	//find item in the "from" list
	while (p) {
		if (p == item) {

			//pop
			if (pprev) {
				pprev->next = p->next;
			}
			else {
				*fromlist = p->next;
			}

			//place in head of "to" list
			p->next = *tolist;
			*tolist = p;

			success = 1;
			break;
		}

		pprev = p;
		p = p->next;
	}

	return success;
}

static void NetTaskResponse (const NetworkResponse_t * response, void * context) {
	FileUploadListItem_t * p = (FileUploadListItem_t *)context;

	//delete even if upload was not successful

	LOGI("done uploading %s\n",p->sfilepath);

	if (p->deleteAfterUpload) {
		FRESULT res;

		res = hello_fs_unlink(p->sfilepath);

		if (res != FR_OK) {
			LOGI("failed to delete file %s\r\n",p->sfilepath);
		}
	}

	//guard the list
	xSemaphoreTake(_listMutex,portMAX_DELAY);

	//free this up
	MoveItemToFront(&_pFreeHead,&_pWorkingHead,p);

	xSemaphoreGive(_listMutex);

	//we have freed up a resource
	xSemaphoreGive(_sem);
}



bool encode_file (pb_ostream_t * stream, const pb_field_t * field,void * const *arg) {
	FRESULT res;
	FIL file_obj;
	FILINFO file_info;

	uint8_t readbuf[256];

	DWORD bytes_to_be_read;
	WORD read_size;
	WORD bytes_read;

	bool success = false;

	const FileUploadListItem_t * p = (const FileUploadListItem_t * )(*arg);


	memset(&file_info,0,sizeof(file_info));
	memset(&file_obj,0,sizeof(file_obj));
	memset(&res,0,sizeof(res));

	//write tag
	if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)) {
		return false;
	}


	/* open file */
	res = hello_fs_open(&file_obj, p->sfilepath, FA_READ);

	/*  Did something horrible happen?  */
	if(res != FR_OK){
		LOGI("File open %s failed: %d\n", p->sfilepath, res);
	}
	else {
		//find out size of file
		hello_fs_stat(p->sfilepath, &file_info);

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


static uint32_t FileEncode(pb_ostream_t * stream,void * data) {
	FileMessage pbfile;

	memset(&pbfile,0,sizeof(pbfile));

	pbfile.device_id.funcs.encode = encode_device_id_string;

	pbfile.unix_time = get_time();
	pbfile.has_unix_time = true;

	pbfile.data.funcs.encode = encode_file;
	pbfile.data.arg = data;

	return pb_encode(stream,FileMessage_fields,&pbfile);
}


static void Enqueue(FileUploadListItem_t * p,uint8_t * recvbuf,uint32_t recvbuf_size) {
	NetworkTaskServerSendMessage_t mnet;

	memset(&mnet,0,sizeof(mnet));

	mnet.decode_buf = recvbuf;
	mnet.decode_buf_size = recvbuf_size;
	mnet.endpoint = p->endpoint;
	mnet.host = p->host;
	mnet.response_callback = NetTaskResponse;

	mnet.encode = FileEncode;
	mnet.encodedata = p;

	mnet.retry_timeout = RETRY_TIMEOUT_TICKS;
	mnet.context = p;

	//the network task will eventually call our encode callback
	//and send the protobuf onwards to the server
	NetworkTask_AddMessageToQueue(&mnet);
}

void FileUploaderTask_Thread(void * data) {
	uint8_t recvbuf[256];
	FileUploadListItem_t * p;
	FileUploadListItem_t * pnext;


	Init();

	for (; ; ) {
		vTaskDelay(POLLING_PERIOD_IN_TICKS);

		//guard the list
		xSemaphoreTake(_listMutex,portMAX_DELAY);

		p = _pCountdownHead;

		//service the list
		while (p) {
			p->delay_in_ticks -= POLLING_PERIOD_IN_TICKS;

#ifdef FILUPLOADER_DEBUG_ME
			LOGI("%s has %d ticks until upload\n",p->sfilepath,p->delay_in_ticks);
#endif

			pnext = p->next;

			if (p->delay_in_ticks <= 0) {
				//move to head of working list
				MoveItemToFront(&_pWorkingHead,&_pCountdownHead,p);

#ifdef FILUPLOADER_DEBUG_ME
				LOGI("trying to upload %s\n",p->sfilepath);
#endif
				//now add to the net task
				Enqueue(p,recvbuf,sizeof(recvbuf));

			}

			p = pnext;
		}


		xSemaphoreGive(_listMutex);


	}
}

uint8_t FileUploaderTask_Upload(const char * filepath,const char * host, const char * endpoint,uint8_t deleteAfterUpload,
		uint32_t group,int32_t delayInTicks,FileUploadNotification_t onUploaded, void * context,uint32_t maxWaitIfBusy) {

	//block if we are out of resources
	if (xSemaphoreTake(_sem,maxWaitIfBusy)) {


		//guard the list
		xSemaphoreTake(_listMutex,portMAX_DELAY);

		if (_pFreeHead) {
			_pFreeHead->context = context;
			_pFreeHead->delay_in_ticks = delayInTicks;
			_pFreeHead->deleteAfterUpload = deleteAfterUpload;
			_pFreeHead->endpoint = endpoint;
			_pFreeHead->group_id = group;
			_pFreeHead->host = host;
			_pFreeHead->onFinished = onUploaded;
			strncpy(_pFreeHead->sfilepath,filepath,MAX_FILEPATH_LEN);

			//this pops from the free list, and moves this item to the countdown head
			MoveItemToFront(&_pCountdownHead,&_pFreeHead,_pFreeHead);
		}
		else {
			LOGE("linked list head is NULL");
		}

		xSemaphoreGive(_listMutex);

	}
	else {
		return 0;
	}

	return 1;





}

void FileUploaderTask_CancelUploadByGroup(uint32_t group) {
	FileUploadListItem_t * p;
	FileUploadListItem_t * pnext;


	//guard the list
	xSemaphoreTake(_listMutex,portMAX_DELAY);

	p = _pCountdownHead;

	while (p) {
		pnext = p->next;

		if (p->group_id == group) {
			//move to free list if group matches -- i.e. cancel this upload
			LOGI("canceling upload of %s\n",p->sfilepath);
			MoveItemToFront(&_pFreeHead,&_pCountdownHead,p);

			if (p->deleteAfterUpload) {
				FRESULT res;

				res = hello_fs_unlink(p->sfilepath);

				if (res != FR_OK) {
					LOGI("failed to delete file %s",p->sfilepath);
				}
			}
		}

		p = pnext;
	}



	xSemaphoreGive(_listMutex);


}

