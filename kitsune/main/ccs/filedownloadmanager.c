#include "filedownloadmanager.h"
#include "wifi_cmd.h"
#include "file_manifest.pb.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"

#define FILE_ERROR_QUEUE_DEPTH (10)

// Response received semaphore
static xSemaphoreHandle _response_received_sem = NULL;

// File error queue
static xQueueHandle _file_error_queue = NULL;

// file download status flag
static bool file_download_status = false; // true if download is in progress, false otherwise.
// mutex to protect file download status
static xSemaphoreHandle _file_download_mutex = NULL;

// link health - send and recv errors
static FileManifest_LinkHealth link_health = {0};

// Static Functions
static void DownloadManagerTask(void * filesyncdata);
static void _get_file_sync_response(pb_field_t ** fields, void ** structdata);
static void _free_file_sync_response(void * structdata);
static void _on_file_sync_response_success( void * structdata);
static void _on_file_sync_response_failure( );

// Init
void downloadmanagertask_init(uint16_t stack_size)
{
	// init error queue
	if (!_file_error_queue) {
		_file_error_queue = xQueueCreate(FILE_ERROR_QUEUE_DEPTH,sizeof(FileManifest_FileOperationError));
	}

	// init mutex for file download status flag
	if(!_file_download_mutex)
	{
		_file_download_mutex = xSemaphoreCreateMutex();
	}

	// init sempahore that indicates server response has been received
	if (!_response_received_sem) {
		_response_received_sem = xSemaphoreCreateBinary();
	}


	//this task needs a larger stack because
	//some protobuf encoding will happen on the stack of this task
	xTaskCreate(DownloadManagerTask, "downloadManagerTask", stack_size, NULL, 2, NULL);


}

// Download Manager task
static void DownloadManagerTask(void * filesyncdata)
{

	// init query delay

	// init link health error count
	link_health.send_errors = 0;
	link_health.time_to_response = 0;

	// init file download status flag
	xSemaphoreTake(_file_download_mutex, portMAX_DELAY);
		file_download_status = false;
	xSemaphoreGive(_file_download_mutex);


	protobuf_reply_callbacks pb_cb;

    pb_cb.get_reply_pb = _get_file_sync_response;
    pb_cb.free_reply_pb = _free_file_sync_response;
    pb_cb.on_pb_success = _on_file_sync_response_success;
    pb_cb.on_pb_failure = _on_file_sync_response_failure;


    //give sempahore with query delay as 15 minutes

	for (; ;)
	{
		//if(xSemaphoreTake(_response_received_sem,portMAX_DELAY) == pdTrue)
		{
			// save time to response count

			// update query delay

			//vTaskDelayUntil() // TODO should this be vtaskdelay instead?

			// Scan through file system and update file manifest

			// Empty file operation error queue

			// Update pb

			// Send pb
			LOGI("Sending file sync \r\n");

			/*
			while( NetworkTask_SendProtobuf(
					true, DATA_SERVER, FILE_SYNC_ENDPOINT, FileManifest_fields, data, to, NULL, NULL, &pb_cb, forced)
					!= 0 )
			*/
			{
				LOGI("File manifest failed to upload \n");

				// Update error count
			}

			// Clear time to response count

			// Update previous wake time
		}
		//else
		{
			// Update time to resposne count
		}


	}
}

// Send file manifest, update pb callbacks

static void _get_file_sync_response(pb_field_t ** fields, void ** structdata)
{
	*fields = (pb_field_t *)FileManifest_fields;
	*structdata = pvPortMalloc(sizeof(FileManifest));
	assert(*structdata);
	if( *structdata ) {
		FileManifest * response_protobuf = *structdata;
		memset(response_protobuf, 0, sizeof(FileManifest));
		//todo
		//response_protobuf;
	}
}
static void _free_file_sync_response(void * structdata)
{
	vPortFree( structdata );
}

static void _on_file_sync_response_success( void * structdata)
{
	LOGF("_on_file_sync_response_success\r\n");

	// give semaphore for query delay
}
static void _on_file_sync_response_failure( )
{
    LOGF("_on_file_sync_response_failure\r\n");

    // give semaphore for default query delay

    // update link health error count
    //TODO is there any way to know if error occurred during send or recv
}
