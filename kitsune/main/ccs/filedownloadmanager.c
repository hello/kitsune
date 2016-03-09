#include "filedownloadmanager.h"
#include "wifi_cmd.h"
#include "file_manifest.pb.h"
#include "queue.h"
#include "semphr.h"

// File error queue

// file download status flag
// mutex to protect file download status

// link health - send and recv errors

// Static Functions
static void DownloadManagerTask(void * filesyncdata);
static void _get_file_sync_response(pb_field_t ** fields, void ** structdata);
static void _free_file_sync_response(void * structdata);
static void _on_file_sync_response_success( void * structdata);
static void _on_file_sync_response_failure( );

// Init
void downloadmanagertask_init(uint16_t stack_size)
{
	//this task needs a larger stack because
	//some protobuf encoding will happen on the stack of this task
	xTaskCreate(DownloadManagerTask, "downloadManagerTask", stack_size, NULL, 2, NULL);

	// init error queue

	// init query delay

	// init link health error count


}

// Download Manager task
static void DownloadManagerTask(void * filesyncdata)
{
	protobuf_reply_callbacks pb_cb;

    pb_cb.get_reply_pb = _get_file_sync_response;
    pb_cb.free_reply_pb = _free_file_sync_response;
    pb_cb.on_pb_success = _on_file_sync_response_success;
    pb_cb.on_pb_failure = _on_file_sync_response_failure;


    //give sempahore with query delay as 15 minutes

	for (; ;)
	{
		//if(xSemaphoreTake() == pdTrue)
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
