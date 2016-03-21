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
#include "hellofilesystem.h"
#include <ustdlib.h>

#define DM_TESTING

#define FILE_ERROR_QUEUE_DEPTH (5)		// ensure that this matches the max_count for error_info in file_manifest.options

// TODO REMEMBER TO CHANGE THIS *******
#ifdef DM_TESTING
	#define QUERY_DELAY_DEFAULT		(1UL) //minutes
#else
	#define QUERY_DELAY_DEFAULT		(15UL) //minutes
#endif

#define PATH_BUF_MAX_SIZE		(64)

//From ff.f of fatfs TCHAR	fname[13];		/* Short file name (8.3 format) */
#define MAX_FILENAME_SIZE		(13)

#define FOLDERS_TO_EXCLUDE      (2)

char* folders[FOLDERS_TO_EXCLUDE] = {"LOGS", "USR"};

typedef struct {
	//File path
	char path[PATH_BUF_MAX_SIZE];

	//Filename
	char filename[MAX_FILENAME_SIZE];
}file_list_t;

typedef struct {

	file_list_t* ga_file_list;
    uint32_t num_data;

    // Indicates the number of files for which memory has been allocated.
    // This refers to the number of files names that can be stored at ga_file_list
    // This number is incremented whenever a realloc is done.
    uint32_t allocated_file_list_size;

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

static uint32_t message_sent_time;

// Query delay in ticks
TickType_t query_delay_ticks = (QUERY_DELAY_DEFAULT*60*1000)/portTICK_PERIOD_MS;

// Static Functions
static void DownloadManagerTask(void * filesyncdata);
static void _get_file_sync_response(pb_field_t ** fields, void ** structdata);
static void _free_file_sync_response(void * structdata);
static void _on_file_sync_response_success( void * structdata);
static void _on_file_sync_response_failure(void );
static void get_file_download_status(FileManifest_FileStatusType* file_status);
bool _on_file_update(pb_istream_t *stream, const pb_field_t *field, void **arg);
static void free_file_sync_info(FileManifest_FileDownload * download_info);
static void restart_download_manager(void);
bool encode_file_info (pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
static uint32_t update_file_manifest(void);
static uint32_t scan_files(char* path);
static int32_t compute_sha(char* path, char* sha_path);
static bool get_sha_filename(char* filename, char* sha_fn);
static int get_complete_filename(char* full_path, char * local_fn, char* path, uint32_t len);
static bool does_sha_file_exist(char* sha_path);
static int get_sha_from_file(char* filename, char* path, uint8_t* sha);

// extern functions
bool send_to_download_queue(SyncResponse_FileDownload* data, TickType_t ticks_to_wait);
bool _decode_string_field(pb_istream_t *stream, const pb_field_t *field, void **arg);
uint32_t get_free_space(uint32_t* free_space, uint32_t* total_mem);
bool _encode_string_fields(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);

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

	xTaskCreate(DownloadManagerTask, "downloadManagerTask", stack_size, NULL, 2, NULL);

	file_manifest_local.ga_file_list = NULL;

	file_manifest_local.allocated_file_list_size = 0;

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
	update_file_download_status(false);

	protobuf_reply_callbacks pb_cb;

    pb_cb.get_reply_pb = _get_file_sync_response;
    pb_cb.free_reply_pb = _free_file_sync_response;
    pb_cb.on_pb_success = _on_file_sync_response_success;
    pb_cb.on_pb_failure = _on_file_sync_response_failure;

    // set current time
    start_time = xTaskGetTickCount();

    //give sempahore with query delay as 15 minutes
    restart_download_manager();

    //LOGI("Starting download manager\n");

    /*
    // Run this once so that most memory realloc and SHA file creation is done
	uint32_t ret = update_file_manifest();
	if(ret)
	{
		LOGE("Error creating file manifest: %d\n", ret);
	}

	*/
	for (; ;)
	{
		// Check if response has been received
		if(xSemaphoreTake(_response_received_sem,portMAX_DELAY))
		{

			// TODO should this be vtaskdelay instead?
			vTaskDelayUntil(&start_time,query_delay_ticks);

			memset(&message_for_upload,0,sizeof(FileManifest));

			/* UPDATE FILE INFO */

			// Scan through file system and update file manifest
			uint32_t ret = update_file_manifest();
			if(ret)
			{
				LOGE("Error creating file manifest: %d\n", ret);
				//TODO what is to be sent if this function returns an error
			}

			message_for_upload.file_info.funcs.encode = encode_file_info;
			message_for_upload.file_info.arg = &file_manifest_local;

			/* UPDATE FILE STATUS - DOWNLOADED/PENDING */

			message_for_upload.has_file_status = true;
			get_file_download_status(&message_for_upload.file_status);

			/* UPDATE DEVICE UPTIME */

			message_for_upload.has_device_uptime_in_seconds = true;
			message_for_upload.device_uptime_in_seconds = xTaskGetTickCount() / configTICK_RATE_HZ; // in seconds

			/* UPDATE QUERY DELAY */

			message_for_upload.has_query_delay = false;

			/* UPDATE FW VERSION */

			message_for_upload.has_firmware_version = true;
#ifdef DM_TESTING
			message_for_upload.firmware_version = 100; //KIT_VER;
#else
			message_for_upload.firmware_version = KIT_VER;
#endif

			/* UPDATE TIME */

			message_for_upload.unix_time = get_time();
			message_for_upload.has_unix_time = true;

			/* UPDATE LINK HEALTH */

			message_for_upload.has_link_health = true;
			message_for_upload.link_health = link_health;


			/* UPDATE MEMORY INFO */

			message_for_upload.has_sd_card_size = true;
			message_for_upload.sd_card_size.has_free_memory = true;
			message_for_upload.sd_card_size.has_total_memory = true;

			get_free_space(&message_for_upload.sd_card_size.free_memory, &message_for_upload.sd_card_size.total_memory);

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

			//LOGI("DM: Sending file sync \r\n");

			message_sent_time = xTaskGetTickCount();

			if(!NetworkTask_SendProtobuf(
					true, DATA_SERVER, FILE_SYNC_ENDPOINT, FileManifest_fields, &message_for_upload, 0, NULL, NULL, &pb_cb, false)
					)
			{
				LOGI("DM: File manifest failed to upload \n");

			}

			//LOGI("DM: file upload sent\n");


			// Update wake time
			start_time = xTaskGetTickCount();
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

		response_protobuf->file_info.funcs.decode = _on_file_update;
		response_protobuf->file_info.arg = NULL;
		response_protobuf->sense_id.funcs.decode = _decode_string_field;
		response_protobuf->sense_id.arg = NULL;

	}
}

//Is it better to handle this in response success
// Better to handle it here since the flags are a part of the structure. and then populate download info and send to queue
bool _on_file_update(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
	//LOGI("DM: file sync parsing\n" );

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
	//LOGI("DM: file sync parsing\n" );
	if( !pb_decode(stream,FileManifest_File_fields,&file_info) )
	{
		LOGI("DM: file sync - parse fail \n" );
		free_file_sync_info( &file_info.download_info );
		return false;
	}

	if(file_info.has_update_file && file_info.update_file)
	{
		if(file_info.has_delete_file && file_info.delete_file)
		{
			FRESULT res;
			char full_path[PATH_BUF_MAX_SIZE];

			// delete file
			//LOGI("DM: delete file\n" );

			// get complete filename
			get_complete_filename( \
					full_path, 		\
					file_info.download_info.sd_card_filename.arg, \
					file_info.download_info.sd_card_path.arg, \
					PATH_BUF_MAX_SIZE);

			// unlink
			res = hello_fs_unlink(full_path);
			if(res)
			{
				LOGE("DM:delete unsuccessful %s\n",full_path );
			}

			free_file_sync_info( &file_info.download_info );

		}
		else
		{
			//LOGI("DM: update file %s\n", file_info.download_info.sd_card_filename.arg );
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
	else
	{
		free_file_sync_info( &file_info.download_info );
		//LOGI("DM: no file update\n" );

	}


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


bool encode_file_info (pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{

    int i;
    file_info_to_encode * data = *(file_info_to_encode**)arg;
    FileManifest_File file_info = {0};

    char path_local[PATH_BUF_MAX_SIZE];

    for( i = 0; i < data->num_data; ++i ) {

        file_info.has_delete_file = false;
        file_info.has_download_info = true;
        file_info.has_update_file = false;

    	file_info.download_info.sd_card_path.funcs.encode = _encode_string_fields;

    	// To remove the leading slash from path
    	strncpy(path_local, &data->ga_file_list[i].path[1],PATH_BUF_MAX_SIZE);

    	file_info.download_info.sd_card_path.arg = path_local;

    	file_info.download_info.sd_card_filename.funcs.encode = _encode_string_fields;
    	file_info.download_info.sd_card_filename.arg = data->ga_file_list[i].filename;

    	file_info.download_info.has_sha1 = true;
    	get_sha_from_file(data->ga_file_list[i].filename,data->ga_file_list[i].path,file_info.download_info.sha1.bytes);

    	file_info.download_info.sha1.size = 20;

    	//LOGI("Sending: %s/%s %u...%u\n",file_info.sd_card_path.arg, file_info.sd_card_filename.arg,file_info.sha1.bytes[0], file_info.sha1.bytes[19]);

        if(!pb_encode_tag_for_field(stream, field))
        {
            LOGI("DM: encode tag error %s\n", PB_GET_ERROR(stream));
            return false;
        }

        if (!pb_encode_submessage(stream, FileManifest_File_fields, &file_info)){
            LOGI("DM: encode error: %s\n", PB_GET_ERROR(stream));
            return false;
        }

    }


    // free file_manifest to encode
	return true;

}

bool add_to_file_error_queue(char* filename, int32_t err_code, bool write_error)
{
	FileManifest_FileOperationError data;

	// if queue is full, remove oldest
	if(! uxQueueSpacesAvailable(_file_error_queue))
	{
		xQueueReceive(_file_error_queue, &data, 10);
	}

	data.has_err_code = true;
	data.err_code = err_code;

	data.has_err_type = true;
	data.err_type = (write_error) ?
			FileManifest_FileOperationError_ErrorType_WRITE_ERROR : FileManifest_FileOperationError_ErrorType_READ_ERROR;

	// Copy file name
	// data.filename.arg, filename,
	data.filename.funcs.encode = _encode_string_fields;
	data.filename.arg = NULL;

	// add to qyeye
	if( !xQueueSend(_file_error_queue, &data, 10))
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
	FileManifest* response_protobuf = (FileManifest*) structdata;

	LOGF("_on_file_sync_response_success\r\n");

	link_health.time_to_response = (xTaskGetTickCount() - message_sent_time) / configTICK_RATE_HZ /60; // in minutes
	//LOGI("DM: Time to response %d \n", link_health.time_to_response );

	// Update query delay ticks
	if(response_protobuf->has_query_delay)
	{
#ifdef DM_TESTING
	    // Update default query delay ticks
		query_delay_ticks = (QUERY_DELAY_DEFAULT*60*1000)/portTICK_PERIOD_MS;
#else
		query_delay_ticks = (response_protobuf->query_delay * 60 * 1000)/portTICK_PERIOD_MS;
#endif
		LOGI("DM: Query delay %d\n", response_protobuf->query_delay);
	}
	else
	{
	    // Update default query delay ticks
		query_delay_ticks = (QUERY_DELAY_DEFAULT*60*1000)/portTICK_PERIOD_MS;
	}

	// Clear time to response count since it has been sent successfully
	link_health.time_to_response = 0;
	link_health.send_errors = 0;

	restart_download_manager();
}

static void _on_file_sync_response_failure( )
{
    LOGF("_on_file_sync_response_failure\r\n");

    // Update default query delay ticks
	query_delay_ticks = (QUERY_DELAY_DEFAULT*60*1000)/portTICK_PERIOD_MS;

	//Also update time_t-_response by 15 minutes
	link_health.time_to_response = portMAX_DELAY;

    // update link health error count
    //TODO is there any way to know if error occurred during send or recv
	link_health.send_errors++;

	/*NOTE: This semaphore give helps to retry sending file manifest even when there is a failure.
	 * Since retries are handled here, the retry time in count for NetworkTask_sendProtobuf() is set to 0.
	 * If that were to change to >0, then the semapahore below needs to be removed
	 */
    // give semaphore for default query delay
	restart_download_manager();

}

static void get_file_download_status(FileManifest_FileStatusType* file_status)
{
	xSemaphoreTake(_file_download_mutex, portMAX_DELAY);
		*file_status = file_download_status;
	xSemaphoreGive(_file_download_mutex);

}

static inline void restart_download_manager(void)
{
	//LOGI("DM: Sem Give\n");
	xSemaphoreGive(_response_received_sem);
}

static uint32_t update_file_manifest(void)
{
	uint32_t res;
	char path_buf[PATH_BUF_MAX_SIZE] = {0};

	strncpy(path_buf,"/",PATH_BUF_MAX_SIZE);

	file_manifest_local.num_data = 0;
	res = scan_files(path_buf);

//	LOGI("DM: File count: %d Allocated file count: %d\n", \
//			file_manifest_local.num_data, file_manifest_local.allocated_file_list_size);

	// Update SHA1

	return res;

}

static uint32_t scan_files(char* path)
{
    FRESULT res;
    FILINFO fno;
    DIR dir;
    int i;
    int j;
    char *fn;   /* This function assumes non-Unicode configuration */

    res = hello_fs_opendir(&dir, path);                       /* Open the directory */
    if (res == FR_OK) {
        i = strlen(path);
        for (;;) {

            res = hello_fs_readdir(&dir, &fno);                   /* Read a directory item */
            if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
            if (fno.fname[0] == '.') continue;             /* Ignore dot entry */


            for(j=0;j<FOLDERS_TO_EXCLUDE;j++)
            {
                if( !strcmp(fno.fname, folders[j]))
                {
                	LOGI("skipping %s\n",fno.fname );
                	break;
                }

            }

            if(j<FOLDERS_TO_EXCLUDE) continue;

            fn = fno.fname;
            if (fno.fattrib & AM_DIR) {                    /* It is a directory */
                usnprintf(&path[i],PATH_BUF_MAX_SIZE * sizeof(char), "%s", fn);
                res = (FRESULT)scan_files(path);
                path[i] = 0;
                if (res != FR_OK) break;
            } else {                                       /* It is a file. */
                // Skip if file is a sha file
                if(!strstr(fn, ".SHA"))
                {
                	//LOGI("DM File found: %s/%s\n", path, fn, strlen(path), strlen(fn));


                	// Allocate memory if needed
					if( ( file_manifest_local.allocated_file_list_size <= file_manifest_local.num_data ) ||
							( !file_manifest_local.ga_file_list) )
					{
						file_manifest_local.allocated_file_list_size = ( file_manifest_local.allocated_file_list_size ) ?
								file_manifest_local.allocated_file_list_size*2 : 2;

						file_manifest_local.ga_file_list =
								(file_list_t*)pvPortRealloc(file_manifest_local.ga_file_list, file_manifest_local.allocated_file_list_size * sizeof(file_list_t));
					}

					// Clear the strings
					memset(file_manifest_local.ga_file_list[file_manifest_local.num_data].path, 0, sizeof(file_manifest_local.ga_file_list[file_manifest_local.num_data].path));
					memset(file_manifest_local.ga_file_list[file_manifest_local.num_data].filename, 0, sizeof(file_manifest_local.ga_file_list[file_manifest_local.num_data].filename));

					// Copy path and filename
					memcpy(file_manifest_local.ga_file_list[file_manifest_local.num_data].path, path, strlen(path));
					memcpy(file_manifest_local.ga_file_list[file_manifest_local.num_data].filename, fn, strlen(fn));

					file_manifest_local.num_data++;

					char sha_filename[MAX_FILENAME_SIZE];
					char full_path[PATH_BUF_MAX_SIZE];
					char full_path_sha[PATH_BUF_MAX_SIZE];

					// Get name of SHA file
					get_sha_filename(fn, sha_filename);

					// Get complete filename
					if(get_complete_filename(full_path,fn, path, PATH_BUF_MAX_SIZE))
						return ~0;

					if(get_complete_filename(full_path_sha,sha_filename, path, PATH_BUF_MAX_SIZE))
						return ~0;

					// TODO what if the filename is the same, but the file has changed. In that case, the file contains the old SHA
					// which is never updated and the file is always downloaded. Need to update/delete SHA file on download
					// Check if SHA file exists
					if(!does_sha_file_exist(full_path_sha))
					{
						// Compute and store SHA
						compute_sha(full_path, full_path_sha);
					}
                }
                else
                {
                	//LOGI("DM skipping File: %s/%s\n", path, fn, strlen(path), strlen(fn));
                }

            }
            vTaskDelay(50/portTICK_PERIOD_MS);
        }
        hello_fs_closedir(&dir);
    }

    return res;
}

static bool does_sha_file_exist(char* sha_path)
{
	FILINFO info;
	FRESULT res;

	res = hello_fs_stat(sha_path, &info);
	if (res)
	{
		LOGI("DM: File doesnt exist %d\n", res);
		return false;
	}

	return true;

}

#include "crypto.h"

static int32_t compute_sha(char* path, char* sha_path)
{
#define minval( a,b ) a < b ? a : b

	uint8_t sha[SHA1_SIZE] = { 0 };
    static uint8_t buffer[128];
    uint32_t bytes_to_read, bytes_read;
    uint32_t bytes_to_write = 0, bytes_written = 0;
    FIL fp = {0};
    FILINFO info;
    FRESULT res;

    SHA1_CTX sha1ctx;
    SHA1_Init(&sha1ctx);

    //fetch path info
    //LOGI( "computing SHA of %s\n", path);
    res = hello_fs_stat(path, &info);
    if (res) {
        LOGE("DM: error getting file info %d\n", res);
        return -1;
    }


    res = hello_fs_open(&fp, path, FA_READ);
    if (res) {
        LOGE("DM: error opening file for read %d\n", res);
        return -1;
    }

    //compute sha
    bytes_to_read = info.fsize;
    while (bytes_to_read > 0) {
		res = hello_fs_read(&fp, buffer,(minval(sizeof(buffer),bytes_to_read)), &bytes_read);
		if (res) {
			LOGE("DM: error reading file %d\n", res);
			return -1;
		}
		SHA1_Update(&sha1ctx, buffer, bytes_read);
		bytes_to_read -= bytes_read;
    }
    hello_fs_close(&fp);
    SHA1_Final(sha, &sha1ctx);

    memset(&fp,0,sizeof(fp));

	// Open for writing
	res = hello_fs_open(&fp, sha_path, FA_OPEN_ALWAYS|FA_WRITE);
	if (res) {
		LOGE("DM: error opening file for write %d\n", res);
		return -1;
	}

	bytes_to_write = SHA1_SIZE;

	while(bytes_to_write > 0)
	{
		// Write SHA into it
		res = hello_fs_write(&fp, sha,SHA1_SIZE, &bytes_written);
		if (res) {
			LOGE("DM: error reading file %d\n", res);
			return -1;
		}

		bytes_to_write -= bytes_written;
	}

    // Close file
    hello_fs_close(&fp);

    return 0;
}

// len is the length of the full_path array
static int32_t get_complete_filename(char* full_path, char * local_fn, char* path, uint32_t len)
{
	if(full_path && local_fn && path)
	{
		strncpy(full_path,path,len);
		strncat(full_path, "/", len);
		strncat(full_path, local_fn, len);

		//LOGI("DM: %s\n", full_path);

		// TODO add some boundary checks
		/*
		// First, check to make sure that the current path (CWD), plus the file
		// name, plus a separator and trailing null, will all fit in the temporary
		// buffer that will be used to hold the file name.  The file name must be
		// fully specified, with path, to FatFs.
		if(strlen(cwd_buff) + strlen(local_fn) + 1 + 1 > sizeof(path_buff))
		{
			LOGI("Resulting path name is too long\n");
			return(0);
		}

		*/

		return 0;
	}

	return -1;
}

// Function that gives SHA filename from actual filename
static bool get_sha_filename(char* filename, char* sha_fn)
{
	char filename_local[MAX_FILENAME_SIZE];
	char* token;

	strncpy(filename_local,filename, MAX_FILENAME_SIZE);

	if(filename && sha_fn)
	{
		strncpy(filename_local,filename, MAX_FILENAME_SIZE);

		// Get token from filename
		token = strtok(filename_local, ".");

		// assumes that pointer sha_fn points to an array of length MAX_FILENAME_SIZE or greater
		// Append .sha
		strncpy(sha_fn,token, MAX_FILENAME_SIZE);
		strncat(sha_fn,".SHA",sizeof(".SHA"));

		return true;
	}

	return false;
}


static int get_sha_from_file(char* filename, char* path, uint8_t* sha)
{
	char sha_filename[MAX_FILENAME_SIZE];
	char sha_fullpath[PATH_BUF_MAX_SIZE];
	FRESULT res;
    uint32_t bytes_to_read, bytes_read;
    FIL fp = {0};

	//get SHA filename
	get_sha_filename(filename,sha_filename);

	// compute full path
	if(get_complete_filename(sha_fullpath,sha_filename,path, PATH_BUF_MAX_SIZE))
	{
		return -1;
	}

	//open file for read
	res = hello_fs_open(&fp, sha_fullpath, FA_READ);
	if (res) {
		//LOGE("DM: error opening file for read %d\n", res);
		return -1;
	}

	// read into SHA
	bytes_to_read = SHA1_SIZE;
	while (bytes_to_read > 0) {
		res = hello_fs_read(&fp, sha,bytes_to_read, &bytes_read);
		if (res) {
			//LOGE("DM: error reading file %d\n", res);
			return -1;
		}
		bytes_to_read -= bytes_read;
	}
	hello_fs_close(&fp);

	//LOGI("Sha calculated: %02x ... %02x\r\n", sha[0], sha[SHA1_SIZE-1]);

	return 0;
}
