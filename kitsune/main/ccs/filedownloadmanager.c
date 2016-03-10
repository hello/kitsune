#include "filedownloadmanager.h"
#include "wifi_cmd.h"
#include "file_manifest.pb.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "proto_utils.h"
#include "kitsune_version.h"
#include "sys_time.h"
#include "limits.h"
#include "networktask.h"

#define FILE_ERROR_QUEUE_DEPTH (10)
#define QUERY_DELAY_DEFAULT		(15) //minutes

// Response received semaphore
static xSemaphoreHandle _response_received_sem = NULL;

// File error queue
static xQueueHandle _file_error_queue = NULL;

// file download status flag
static FileManifest_FileStatusType file_download_status = FileManifest_FileStatusType_DOWNLOAD_COMPLETED;
// mutex to protect file download status
static xSemaphoreHandle _file_download_mutex = NULL;

// link health - send and recv errors
static FileManifest_LinkHealth link_health = {0};

// Query delay in ticks
TickType_t query_delay_ticks = (QUERY_DELAY_DEFAULT*60*1000)/portTICK_PERIOD_MS;

// Static Functions
static void DownloadManagerTask(void * filesyncdata);
static void _get_file_sync_response(pb_field_t ** fields, void ** structdata);
static void _free_file_sync_response(void * structdata);
static void _on_file_sync_response_success( void * structdata);
static void _on_file_sync_response_failure(void );
static void get_file_download_status(FileManifest_FileStatusType* file_status);

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


	//stack_size? //TODO
	xTaskCreate(DownloadManagerTask, "downloadManagerTask", stack_size, NULL, 2, NULL);


}

void set_file_download_pending(void)
{
	xSemaphoreTake(_file_download_mutex, portMAX_DELAY);
		file_download_status = FileManifest_FileStatusType_DOWNLOAD_PENDING;
	xSemaphoreGive(_file_download_mutex);

}



// Download Manager task
static void DownloadManagerTask(void * filesyncdata)
{
	static TickType_t start_time;
	const uint32_t response_wait_time = 60 * 1000; // 1 minute in ms
	const TickType_t response_wait = response_wait_time/portTICK_PERIOD_MS;
	FileManifest_FileOperationError error_message;
	FileManifest message_for_upload;

	// init query delay - Set default delay of 15 minutes
	query_delay_ticks = (QUERY_DELAY_DEFAULT*60*1000)/portTICK_PERIOD_MS;

	// init link health error count
	link_health.send_errors = 0;
	link_health.time_to_response = 0;
	link_health.has_send_errors = true;
	link_health.has_time_to_response = true;

	// init file download status flag
	xSemaphoreTake(_file_download_mutex, portMAX_DELAY);
		file_download_status = FileManifest_FileStatusType_DOWNLOAD_COMPLETED;
	xSemaphoreGive(_file_download_mutex);


	protobuf_reply_callbacks pb_cb;

    pb_cb.get_reply_pb = _get_file_sync_response;
    pb_cb.free_reply_pb = _free_file_sync_response;
    pb_cb.on_pb_success = _on_file_sync_response_success;
    pb_cb.on_pb_failure = _on_file_sync_response_failure;


    // set current time
    start_time = xTaskGetTickCount();

    //give sempahore with query delay as 15 minutes
    xSemaphoreGive(_response_received_sem);

	for (; ;)
	{
		// Check if response has been received
		if(xSemaphoreTake(_response_received_sem,response_wait))
		{
			vTaskDelayUntil(&start_time,query_delay_ticks); // TODO should this be vtaskdelay instead?

			/* UPDATE FILE INFO */

			// Scan through file system and update file manifest

			/* UPDATE FILE STATUS - DOWNLOADED/PENDING */

			message_for_upload.has_file_status = true;
			get_file_download_status(&message_for_upload.file_status);


			/* UPDATE DEVICE UPTIME */

			message_for_upload.has_device_uptime = true;
			message_for_upload.device_uptime = xTaskGetTickCount() / configTICK_RATE_HZ; // in seconds

			/* UPDATE FW VERSION */

			message_for_upload.has_firmware_version = true;
			message_for_upload.firmware_version = KIT_VER;

			/* UPDATE TIME */

			message_for_upload.unix_time = get_time();
			message_for_upload.has_unix_time = true;

			/* UPDATE LINK HEALTH */

			message_for_upload.has_link_health = true;
			message_for_upload.link_health = link_health;

			// Clear time to response count since it has been saved to protobuf
			link_health.time_to_response = 0;

			/* UPDATE MEMORY INFO */

			/* UPDATE FILE ERROR INFO */

			// Empty file operation error queue
			while(xQueueReceive( _file_error_queue, &error_message, 0 ))
			{
				// Add to protobuf
			}

			/* UPDATE SENSE ID */

			message_for_upload.sense_id.funcs.encode = encode_device_id_string;

			/* SEND PROTOBUF */

			LOGI("Sending file sync \r\n");


			//TODO are all parameters right
			if(NetworkTask_SendProtobuf(
					true, DATA_SERVER, FILE_SYNC_ENDPOINT, FileManifest_fields, &message_for_upload, INT_MAX, NULL, NULL, &pb_cb, false)
					!= 0 )
			{
				LOGI("File manifest failed to upload \n");

				// Update error count
			}

			// Update wake time
			start_time = xTaskGetTickCount();
		}
		else
		{
			// Update time to response count
			link_health.time_to_response++;
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

	// If has update file and delete file info
		// send for download

	// Update query delay ticks

	// give semaphore for query delay
	xSemaphoreGive(_response_received_sem);
}

static void _on_file_sync_response_failure( )
{
    LOGF("_on_file_sync_response_failure\r\n");

    // Update default query delay ticks
	query_delay_ticks = (QUERY_DELAY_DEFAULT*60*1000)/portTICK_PERIOD_MS;

    // give semaphore for default query delay
	xSemaphoreGive(_response_received_sem);

    // update link health error count
    //TODO is there any way to know if error occurred during send or recv
}

static void get_file_download_status(FileManifest_FileStatusType* file_status)
{
	xSemaphoreTake(_file_download_mutex, portMAX_DELAY);
		*file_status = file_download_status;
	xSemaphoreGive(_file_download_mutex);

}

