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
#include <string.h>

#define FILE_ERROR_QUEUE_DEPTH (10)		// ensure that this matches the max_count for error_info in file_manifest.options
#define QUERY_DELAY_DEFAULT		(15UL) //minutes

typedef struct {
	FileManifest_FileDownload * data;
    uint32_t num_data;
} file_info_to_encode;

// Holds the file info that will be encoded into pb
static file_info_to_encode file_manifest_local;

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
//TODO may need a mutex to protect send_error?

// Query delay in ticks
TickType_t query_delay_ticks = (QUERY_DELAY_DEFAULT*60*1000)/portTICK_PERIOD_MS;

// Static Functions
static void DownloadManagerTask(void * filesyncdata);
static void _get_file_sync_response(pb_field_t ** fields, void ** structdata);
static void _free_file_sync_response(void * structdata);
static void _on_file_sync_response_success( void * structdata);
static void _on_file_sync_response_failure(void );
static void get_file_download_status(FileManifest_FileStatusType* file_status);
static bool _on_file_update(pb_istream_t *stream, const pb_field_t *field, void **arg);
static void free_file_sync_info(FileManifest_FileDownload * download_info);
static void restart_download_manager(void);
bool encode_file_info (pb_ostream_t *stream, const pb_field_t *field, void * const *arg);

// extern functions
bool send_to_download_queue(SyncResponse_FileDownload* data, TickType_t ticks_to_wait);
bool _decode_string_field(pb_istream_t *stream, const pb_field_t *field, void **arg);

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

void update_file_download_status(bool is_pending)
{
	xSemaphoreTake(_file_download_mutex, portMAX_DELAY);
		file_download_status = (is_pending) ? FileManifest_FileStatusType_DOWNLOAD_PENDING:FileManifest_FileStatusType_DOWNLOAD_COMPLETED;
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
    restart_download_manager();

	for (; ;)
	{
		// Check if response has been received
		if(xSemaphoreTake(_response_received_sem,response_wait))
		{
			vTaskDelayUntil(&start_time,query_delay_ticks); // TODO should this be vtaskdelay instead?

			/* UPDATE FILE INFO */

			// Scan through file system and update file manifest

			message_for_upload.file_info.funcs.encode = encode_file_info;
			message_for_upload.file_info.arg = &file_manifest_local;

			/* UPDATE FILE STATUS - DOWNLOADED/PENDING */

			message_for_upload.has_file_status = true;
			get_file_download_status(&message_for_upload.file_status);

			/* Update query delay */
			message_for_upload.has_query_delay = false;

			/* UPDATE DEVICE UPTIME */

			message_for_upload.has_device_uptime_in_seconds = true;
			message_for_upload.device_uptime_in_seconds = xTaskGetTickCount() / configTICK_RATE_HZ; // in seconds

			/* UPDATE FW VERSION */

			message_for_upload.has_firmware_version = true;
			message_for_upload.firmware_version = KIT_VER; //TODO is this right

			/* UPDATE TIME */

			message_for_upload.unix_time = get_time();
			message_for_upload.has_unix_time = true;

			/* UPDATE LINK HEALTH */

			message_for_upload.has_link_health = true;
			message_for_upload.link_health = link_health;

			// Clear time to response count since it has been saved to protobuf
			link_health.time_to_response = 0;
			link_health.send_errors = 0;

			/* UPDATE MEMORY INFO */

			message_for_upload.has_sd_card_size = true;
			//TODO update memory info

			/* UPDATE FILE ERROR INFO */

			message_for_upload.error_info_count = 0;

			// Empty file operation error queue
			while(xQueueReceive( _file_error_queue, &error_message, 0 ))
			{
				// Add to protobuf
				message_for_upload.error_info[message_for_upload.error_info_count] = error_message;
				message_for_upload.error_info_count++;

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

				// give semaphore here to restart sending
				restart_download_manager();

				// Update error count
			}
			else
			{
				// clear error count
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
		//todo - is this right, do i need more callbacks
		response_protobuf->file_info.funcs.decode = _on_file_update;

		//todo callback for error info and sense id ? Since this wont be sent by server, is this needed?
	}
}

//TODO update this callback This needs to be called only if response has update.
//Is it better to handle this in response success
// Better to handle it here since the flags are a part of the structure. and then populate download info and send to queue
bool _on_file_update(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
	FileManifest_File file_info;

	file_info.download_info.host.funcs.decode = _decode_string_field;
	file_info.download_info.host.arg = NULL;

	file_info.download_info.url.funcs.decode = _decode_string_field;
	file_info.download_info.url.arg = NULL;

	file_info.download_info.sd_card_filename.funcs.decode = _decode_string_field;
	file_info.download_info.sd_card_filename.arg = NULL;

	file_info.download_info.sd_card_path.funcs.decode = _decode_string_field;
	file_info.download_info.sd_card_path.arg = NULL;


	// decode PB
	LOGI("file sync parsing\n" );
	if( !pb_decode(stream,FileManifest_File_fields,&file_info) )
	{
		LOGI("file sync - parse fail \n" );
		free_file_sync_info( &file_info.download_info );
		return false;
	}

	if(file_info.has_update_file && file_info.update_file)
	{
		if(file_info.has_delete_file && file_info.delete_file)
		{
			// delete file
		}
		else
		{
			// Download file
			// Prepare download info and send to download task

			SyncResponse_FileDownload download_info = {0};

			download_info.host.arg = file_info.download_info.host.arg;
			download_info.url.arg = file_info.download_info.url.arg;
			download_info.sd_card_path.arg = file_info.download_info.sd_card_path.arg;
			download_info.sd_card_filename.arg = file_info.download_info.sd_card_filename.arg;
			download_info.has_sha1 = file_info.download_info.has_sha1;
			memcpy(&download_info.sha1, &file_info.download_info.sha1, sizeof(FileManifest_FileDownload_sha1_t));


			if( !send_to_download_queue(&download_info,10) )
			{
				free_file_sync_info( &file_info.download_info );
				return false;

			}

		}
	}

	free_file_sync_info( &file_info.download_info );
	return true;
}

void free_file_sync_info(FileManifest_FileDownload * download_info)
{
	char * filename=NULL, * url=NULL, * host=NULL, * path=NULL;

	filename = download_info->sd_card_filename.arg;
	path = download_info->sd_card_path.arg;
	url = download_info->url.arg;
	host = download_info->host.arg;

	if( filename ) {
		vPortFree(filename);
	}
	if( url ) {
		vPortFree(url);
	}
	if( host ) {
		vPortFree(host);
	}
	if( path ) {
		vPortFree(path);
	}

}

//TODO
bool encode_file_info (pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{

	return true;

}

bool add_to_file_error_queue(char* filename, int32_t err_code, bool write_error)
{
	FileManifest_FileOperationError data;

	// if queue is full, remove oldest
	if(! uxQueueSpacesAvailable(_file_error_queue))
	{
		xQueueReceive(_file_error_queue, &data, portMAX_DELAY); // TODO portMaxDelay?
	}

	data.has_err_code = true;
	data.err_code = err_code;

	data.has_err_type = true;
	data.err_type = (write_error) ?
			FileManifest_FileOperationError_ErrorType_WRITE_ERROR : FileManifest_FileOperationError_ErrorType_READ_ERROR;

	// Copy file name
	// data.filename.arg, filename,
	data.filename.funcs.encode = _encode_string_fields;

	// add to qyeye
	if( !xQueueSend(_file_error_queue, &data, portMAX_DELAY))
	{
		return false;
	}

	return true;
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

	//Also update time_t-_response by 15 minutes

    // give semaphore for default query delay
	restart_download_manager();

    // update link health error count
    //TODO is there any way to know if error occurred during send or recv
}

static void get_file_download_status(FileManifest_FileStatusType* file_status)
{
	xSemaphoreTake(_file_download_mutex, portMAX_DELAY);
		*file_status = file_download_status;
	xSemaphoreGive(_file_download_mutex);

}


static void restart_download_manager(void)
{
	xSemaphoreGive(_response_received_sem);
}
