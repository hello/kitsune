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

#include "crypto.h"

#include "ble_proto.h"

//#define DM_TESTING

#define FILE_ERROR_QUEUE_DEPTH (10)		// ensure that this matches the max_count for error_info in file_manifest.options

#define QUERY_DELAY_DEFAULT		(15UL) //minutes


#define PATH_BUF_MAX_SIZE		(64)

//From ff.f of fatfs TCHAR	fname[13];		/* Short file name (8.3 format) */
#define MAX_FILENAME_SIZE		(13)

#define FOLDERS_TO_INCLUDE      (2)

char* folders[FOLDERS_TO_INCLUDE] = {"SLPTONES", "RINGTONE"};

typedef enum {
	sha_file_create=0,
	sha_file_delete,
	sha_file_get_sha
}update_sha_t;

typedef FRESULT (*filesystem_rw_func_t)(FIL *, const void *,UINT ,UINT * );


// Semaphore to enable file_sync upload from cli
static xSemaphoreHandle filemngr_upload_sem = NULL;

const uint32_t test_code = 0xAF50AF50;

// Counter to periodically update existing SHA file for each whitelisted file
// This done so that the SHA file has the latest SHA
static uint32_t sha_calc_running_count = 0;

static uint32_t total_file_count = 0;

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

static uint32_t message_sent_time;

// Query delay in ticks
TickType_t query_delay_ticks = (QUERY_DELAY_DEFAULT*60*1000)/portTICK_PERIOD_MS;

// Global functions
uint32_t update_sha_file(char* path, char* original_filename, update_sha_t option, uint8_t* sha, bool ovwr );
bool _on_file_update(pb_istream_t *stream, const pb_field_t *field, void **arg);
bool encode_file_info (pb_ostream_t *stream, const pb_field_t *field, void * const *arg);

// Static Functions
static void DownloadManagerTask(void * filesyncdata);
static void _get_file_sync_response(pb_field_t ** fields, void ** structdata);
static void _free_file_sync_response(void * structdata);
static void _on_file_sync_response_success( void * structdata);
static void _on_file_sync_response_failure(void );
static FileManifest_FileStatusType get_file_download_status(void);
static void free_file_sync_info(FileManifest_FileDownload * download_info);
static void restart_download_manager(void);
static bool scan_files(char* path, pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
static int32_t sd_sha_verifynsave(const char * sha_truth, char* path, char* sha_path);
static bool get_sha_filename(char* filename, char* sha_fn);
static int32_t get_complete_filename(char* full_path, char * local_fn, char* path, uint32_t len);
static bool does_sha_file_exist(char* sha_path);
static uint32_t sd_card_test(bool rw, uint8_t* ptr, filesystem_rw_func_t fs_rw_func);

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


	// Create a folder for SD RW testing
    if(hello_fs_stat("/test", NULL))
	{
    	FRESULT res = hello_fs_mkdir("/test");
		if(res != FR_OK)
		{
			LOGE("DM: Err creating test folder\n");
		}
	}

	xTaskCreate(DownloadManagerTask, "dmTask", stack_size, NULL, 2, NULL);

	if(!filemngr_upload_sem)
	{
		filemngr_upload_sem = xSemaphoreCreateBinary();
	}

}

void update_file_download_status(bool is_pending)
{
	xSemaphoreTake(_file_download_mutex, portMAX_DELAY);
		file_download_status = (is_pending) ? FileManifest_FileStatusType_DOWNLOAD_PENDING:FileManifest_FileStatusType_DOWNLOAD_COMPLETED;
	xSemaphoreGive(_file_download_mutex);

	//start the next one then
	if( !is_pending && xTaskGetTickCount() > 90000 ) {
		xSemaphoreGive(filemngr_upload_sem);
	}
}



// Download Manager task
static void DownloadManagerTask(void * filesyncdata)
{
	FileManifest_FileOperationError error_message;
	FileManifest message_for_upload;

	uint8_t* test_buf;

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

    //give sempahore with query delay as 15 minutes
    restart_download_manager();

	for (; ;)
	{
		// Check if response has been received
		if(xSemaphoreTake(_response_received_sem,portMAX_DELAY))
		{
			retry:
			xSemaphoreTake(filemngr_upload_sem, query_delay_ticks);

			if( get_ble_mode() != BLE_NORMAL ||
					get_file_download_status() == FileManifest_FileStatusType_DOWNLOAD_PENDING ) {
				LOGI("not downloading %d or %d\n", get_ble_mode(), get_file_download_status());
				goto retry;
			}

			memset(&message_for_upload,0,sizeof(FileManifest));

			/* SCAN AND COMPUTE SHA FOR RANDOM FILE */
			char path_buf[PATH_BUF_MAX_SIZE] = {0};

			strncpy(path_buf,"/",PATH_BUF_MAX_SIZE);

			total_file_count = 0;
			scan_files(path_buf, NULL, NULL, NULL);

			/* SD CARD TEST WRITE */
			message_for_upload.sd_card_size.has_sd_card_failure = true;
			test_buf = NULL;
			test_buf = (uint8_t*) pvPortMalloc(SD_BLOCK_SIZE);
			if(!test_buf){
				LOGE("DM test_buf: not enough memory\n");
			}
			else{

				if(sd_card_test(false, test_buf, hello_fs_write))
				{
					LOGE("DM: SD card write err\n");
					message_for_upload.sd_card_size.sd_card_failure = true;
				}
			}

			/* UPDATE FILE INFO */

			message_for_upload.file_info.funcs.encode = encode_file_info;
			message_for_upload.file_info.arg = NULL;

			/* UPDATE FILE STATUS - DOWNLOADED/PENDING */

			message_for_upload.has_file_status = true;
			message_for_upload.file_status = get_file_download_status();

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

			LOGD("DM: LH %d\n",link_health.send_errors);

			/* UPDATE MEMORY INFO */

			message_for_upload.has_sd_card_size = true;
			message_for_upload.sd_card_size.has_free_memory = true;
			message_for_upload.sd_card_size.has_total_memory = true;

			get_free_space(&message_for_upload.sd_card_size.free_memory, &message_for_upload.sd_card_size.total_memory);

			LOGD("+");
			/* UPDATE FILE ERROR INFO */

			message_for_upload.error_info_count = 0;

			// Empty file operation error queue
			while (xQueueReceive(_file_error_queue, &error_message, 0)
					&& message_for_upload.error_info_count < sizeof(message_for_upload.error_info) / sizeof(FileManifest_FileOperationError)) {
				// Add to protobuf
				LOGI("err %d %d\n", message_for_upload.error_info_count, error_message.err_code );
				vTaskDelay(100);
				message_for_upload.error_info[message_for_upload.error_info_count++] = error_message;
			}

			LOGD("-");
			/* UPDATE SENSE ID */
			message_for_upload.sense_id.funcs.encode = encode_device_id_string;

			/* SD CARD TEST READ */
			if(test_buf){
				memset(test_buf,0,SD_BLOCK_SIZE);
				if(sd_card_test(true, test_buf, hello_fs_read))
				{
					LOGE("DM: SD card read err\n");
					message_for_upload.sd_card_size.sd_card_failure = true;
				}
				vPortFree(test_buf);
			}

			/* SEND PROTOBUF */
			LOGD("+");
			message_sent_time = xTaskGetTickCount();
			if(!NetworkTask_SendProtobuf(
					true, DATA_SERVER, FILE_SYNC_ENDPOINT, FileManifest_fields, &message_for_upload, 0, NULL, NULL, &pb_cb, false)
					)
			{
				LOGE("DM: Upload Fail \n");
			}
		}

	}
}

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
	if( !pb_decode(stream,FileManifest_File_fields,&file_info) )
	{
		LOGE("DM: parse fail \n" );
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
			LOGI("DM: delete file\n" );

			// get complete filename
			if( !get_complete_filename( \
					full_path, 		\
					file_info.download_info.sd_card_filename.arg, \
					file_info.download_info.sd_card_path.arg, \
					PATH_BUF_MAX_SIZE)  ){

				// unlink
				res = hello_fs_unlink(full_path);
				if(res)
				{
					LOGE("DM:file delete fail %s\n",full_path );
				}
			}

			free_file_sync_info( &file_info.download_info );

		}
		else
		{
			// Download file
			// Prepare download info and send to download task

			SyncResponse_FileDownload download_info = (SyncResponse_FileDownload)SyncResponse_FileDownload_init_default;

			download_info.host.arg = file_info.download_info.host.arg;
			download_info.url.arg = file_info.download_info.url.arg;
			download_info.sd_card_path.arg = file_info.download_info.sd_card_path.arg;
			download_info.sd_card_filename.arg = file_info.download_info.sd_card_filename.arg;
			download_info.has_sha1 = file_info.download_info.has_sha1;
			memcpy(&download_info.sha1.bytes, &file_info.download_info.sha1.bytes, file_info.download_info.sha1.size);
			download_info.sha1.size = file_info.download_info.sha1.size;

#if DM_TESTING
			LOGI("DM: download file sha %d, ", download_info.sha1.size );
			int i;
			for(i=0;i<download_info.sha1.size;++i) LOGI("%x:",download_info.sha1.bytes[i]);
			LOGI("\n");
#endif


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
		LOGI("DM: no file update\n" );

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


static bool scan_files(char* path, pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
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
        	//LOGI("readdir %s -- %d\n",fno.fname, res );
            if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
            if (fno.fname[0] == '.') continue;             /* Ignore dot entry */

            fn = fno.fname;
            if (fno.fattrib & AM_DIR) {                    /* It is a directory */
                //usnprintf(&path[i],PATH_BUF_MAX_SIZE * sizeof(char), "%s", fn);

                for(j=0;j<FOLDERS_TO_INCLUDE;j++)
                {
                    if( !strcmp(fno.fname, folders[j]))
                    {
                    	LOGI("reading %s\n",fno.fname );
                    	break;
                    }

                }

                if(j==FOLDERS_TO_INCLUDE) {
                	LOGI("skipping %s\n",fno.fname );
                	continue;
                }

                strncat(&path[i],fn,PATH_BUF_MAX_SIZE-strlen(path));
            	//LOGI("DM  recurse in dir %s/%s\n", path, fn);
                res = (FRESULT)scan_files(path,stream,field,arg);
            	//LOGI("DM  recurse ou dir %s/%s\n", path, fn);

                path[i] = 0;
                if (!res) break;
            } else {                                       /* It is a file. */
                // Skip if file is a sha file
                if(!strstr(fn, ".SHA"))
                {
                	//LOGI("DM File found: %s/%s\n", path, fn, strlen(path), strlen(fn));

					if( stream == NULL ) {

						// If downloads are pending, skip SHA calculation
						if(get_file_download_status() != FileManifest_FileStatusType_DOWNLOAD_PENDING){

							vTaskDelay(5);

							bool sha_ovwr = false;

							sha_ovwr = (sha_calc_running_count == total_file_count) ? true : false;

							if(update_sha_file(path,fn, sha_file_create,NULL, sha_ovwr ))
							{
								LOGE("DM: SHA not created %s\n", fn);
							}

							vTaskDelay(5);
						}

						total_file_count++;

            		} else {
            			FileManifest_File file_info = {0};

                   		file_info.has_delete_file = false;
						file_info.has_download_info = true;
						file_info.has_update_file = false;

						file_info.download_info.sd_card_path.funcs.encode = _encode_string_fields;

						// To remove the leading slash from path
						file_info.download_info.sd_card_path.arg = path+1;

						file_info.download_info.sd_card_filename.funcs.encode = _encode_string_fields;
						file_info.download_info.sd_card_filename.arg = fn;

						file_info.download_info.sha1.size = 20;

						file_info.download_info.has_sha1 = !update_sha_file(path, fn, sha_file_get_sha, file_info.download_info.sha1.bytes, false );
	            		if( file_info.download_info.has_sha1 ) {
	            			file_info.download_info.sha1.size = SHA1_SIZE;
#if DM_TESTING
	            			int i;
	            			LOGI("DM UPLOAD SHA ");
	            			for(i=0;i<SHA1_SIZE;++i) LOGI("%x:",file_info.download_info.sha1.bytes[i]);
	            			LOGI("\n");
#endif
	            		}

						if(!pb_encode_tag_for_field(stream, field))
						{
							LOGE("DM: encode tag error %s\n", PB_GET_ERROR(stream));
							return false;
						}

						if (!pb_encode_submessage(stream, FileManifest_File_fields, &file_info)){
							LOGE("DM: encode error: %s\n", PB_GET_ERROR(stream));
							return false;
						}
	                	//LOGI("DM File encoded: %s/%s\n", path, fn);
            		}
                }
                else
                {
                	//LOGI("DM skipping File: %s/%s\n", path, fn, strlen(path), strlen(fn));
                }

            }
        }
    	//LOGI("DM  closing dir %s/%s\n", path, fn);

        hello_fs_closedir(&dir);


    }

    //LOGI("DM  closed dir \n");
    return true;
}


bool encode_file_info (pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
	char path_buf[PATH_BUF_MAX_SIZE] = {0};

	strncpy(path_buf,"/",PATH_BUF_MAX_SIZE);

	return scan_files(path_buf, stream, field, arg);
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
	data.filename.funcs.encode = NULL;
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
	FileManifest* response_protobuf = (FileManifest*) structdata;
	sha_calc_running_count = (++sha_calc_running_count) % total_file_count;

	if( response_protobuf->sense_id.arg ) {
		vPortFree(response_protobuf->sense_id.arg);
	}
	vPortFree( structdata );
}

static void _on_file_sync_response_success( void * structdata)
{
	FileManifest* response_protobuf = (FileManifest*) structdata;

	LOGF("--FILE SYNC SUCCESS--\r\n");

	// TODO Might be more meaningful to have this in seconds
	link_health.time_to_response = (xTaskGetTickCount() - message_sent_time) / configTICK_RATE_HZ /60; // in minutes
	//LOGI("DM: Time to response %d \n", link_health.time_to_response );

	// Update query delay ticks
	if(response_protobuf->has_query_delay)
	{
		query_delay_ticks = (response_protobuf->query_delay * 60 * 1000)/portTICK_PERIOD_MS;

		//LOGI("DM: Query delay %d\n", response_protobuf->query_delay);
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
    LOGF("--FILE SYNC FAIL--\r\n");

    // Update default query delay ticks
	query_delay_ticks = (QUERY_DELAY_DEFAULT*60*1000)/portTICK_PERIOD_MS;

	//Also update time_t-_response by 15 minutes
	link_health.time_to_response = portMAX_DELAY;

    // update link health error count
	link_health.send_errors++;

	/*NOTE: This semaphore-give helps to retry sending file manifest even when there is a failure.
	 * Since retries are handled here, the retry time in count for NetworkTask_sendProtobuf() is set to 0.
	 * If that were to change to >0, then the semapahore below needs to be removed
	 */
    // give semaphore for default query delay
	restart_download_manager();

}

static FileManifest_FileStatusType get_file_download_status(void)
{
	FileManifest_FileStatusType file_status;

	xSemaphoreTake(_file_download_mutex, portMAX_DELAY);
		file_status = file_download_status;
	xSemaphoreGive(_file_download_mutex);

	return file_status;
}

static inline void restart_download_manager(void)
{
	//LOGI("DM: Sem Give\n");
	xSemaphoreGive(_response_received_sem);
}

static bool does_sha_file_exist(char* sha_path)
{
	FRESULT res;

	res = hello_fs_stat(sha_path,NULL);
	if (res)
	{
		return false;
	}

	return true;

}


static int32_t sd_sha_verifynsave(const char * sha_truth, char* path, char* sha_path)
{
#define minval( a,b ) a < b ? a : b

	uint8_t sha[SHA1_SIZE] = { 0 };
    uint8_t* buffer = NULL;
    uint32_t bytes_to_read, bytes_read;
    uint32_t bytes_to_write = 0, bytes_written = 0;
    FIL fp = {0};
    FILINFO info;
    FRESULT res;

    SHA1_CTX sha1ctx;
    SHA1_Init(&sha1ctx);

    buffer = (uint8_t*) pvPortMalloc(SD_BLOCK_SIZE);
    if(!buffer)
    {
    	LOGE("DM: buffer: Not enough memory\n");
        goto fail;
    }

    //fetch path info
    //LOGI( "computing SHA of %s\n", path);
    res = hello_fs_stat(path, &info);
    if (res) {
        LOGE("DM: f_stat %d\n", res);
        goto fail;
    }


    res = hello_fs_open(&fp, path, FA_READ);
    if (res) {
        LOGE("DM: f_open for read %d\n", res);
        goto fail;
    }

    //compute sha
    bytes_to_read = info.fsize;
    while (bytes_to_read > 0) {
		res = hello_fs_read(&fp, buffer,(minval(SD_BLOCK_SIZE,bytes_to_read)), &bytes_read);
		if (res != FR_OK) {
			LOGE("DM: f_read %d\n", res);
			goto fail;
		}
		SHA1_Update(&sha1ctx, buffer, bytes_read);
		bytes_to_read -= bytes_read;
    }

    hello_fs_close(&fp);
    SHA1_Final(sha, &sha1ctx);

    //compare
    if(sha_truth){
    	LOGI("SHA COMPARE\n");
		if (memcmp(sha, sha_truth, SHA1_SIZE) != 0) {
			LOGE("SD card file SHA did not match!\n");
			//LOGI("Sha truth:      %02x ... %02x\r\n", sha_truth[0], sha_truth[SHA1_SIZE-1]);
			//LOGI("Sha calculated: %02x ... %02x\r\n", sha[0], sha[SHA1_SIZE-1]);
			goto fail;
		}
    }


	memset(&fp,0,sizeof(fp));

	// Open for writing
	res = hello_fs_open(&fp, sha_path, FA_CREATE_ALWAYS|FA_WRITE);
	if (res) {
		LOGE("DM: f_open for write %d\n", res);
		goto fail;
	}

	bytes_to_write = SHA1_SIZE;

#if DM_TESTING
	int i;
	LOGI("DM SHA ");
	for(i=0;i<SHA1_SIZE;++i) LOGI("%x:",sha[i]);
	LOGI("\n");
#endif

	while(bytes_to_write > 0)
	{
		// Write SHA into it
		res = hello_fs_write(&fp, sha,SHA1_SIZE, &bytes_written);
		if (res) {
			LOGE("DM: f_write %d\n", res);
			goto fail;
		}

		bytes_to_write -= bytes_written;
	}

	// Close file
	hello_fs_close(&fp);

	if(buffer) vPortFree(buffer);


    return 0;

fail:
	if(buffer) vPortFree(buffer);
	return -1;

}

// len is the length of the full_path array
static int32_t get_complete_filename(char* full_path, char * local_fn, char* path, uint32_t len)
{
	if(full_path && local_fn && path)
	{
		if(strlen(path) + strlen(local_fn) + 1 + 1 > len)
		{
			LOGE("DM: name too long\n");
			return -1;
		}

		strncpy(full_path,path,len);
		strncat(full_path, "/", len);
		strncat(full_path, local_fn, len);

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

// TODO better name for function
uint32_t update_sha_file(char* path, char* original_filename, update_sha_t option, uint8_t* sha, bool ovwr)
{
	char sha_filename[MAX_FILENAME_SIZE];
	char sha_fullpath[PATH_BUF_MAX_SIZE];
	FRESULT res;
	bool file_exists;

	// Get name of SHA file from original filename
	if(get_sha_filename(original_filename, sha_filename) == false)
		return ~0;

	// get absolute path of sha file
	if(get_complete_filename(sha_fullpath,sha_filename, path, PATH_BUF_MAX_SIZE))
		return ~0;

	// Check if SHA file exists
	file_exists = (does_sha_file_exist(sha_fullpath)) ? true: false;

	switch(option)
	{
		case sha_file_create:
		{

			if(file_exists && !ovwr )
			{
				// File exists and overwrite flag is false,
				// no need to update
				return 0;
			}
			LOGI("DM: SHA create %s\n", sha_filename);
			char full_path[PATH_BUF_MAX_SIZE];

			// Get absolute path of original file
			if(get_complete_filename(full_path,original_filename, path, PATH_BUF_MAX_SIZE))
				return ~0;

			// start time
			uint32_t time_for_sha = get_time();
			// Compute and store SHA. This function overwrites an existing SHA file
			//compute_sha(full_path, sha_fullpath);
			if(sd_sha_verifynsave((const char*)sha,full_path,sha_fullpath))
			{
				LOGE("DM: SHA verifynsave fail\n");
				return 1;
			}
			// stop time
			time_for_sha = get_time() - time_for_sha;
			LOGI("DM %s sha_time=%u\n",original_filename,time_for_sha);
		}
		break;

		case sha_file_delete:
		{

			if(file_exists)
			{
				// Delete exisiting SHA file
				LOGI("SHA delete\n");
				res = hello_fs_unlink(sha_fullpath);
				if(res)
				{
					LOGE("DM:delete fail %s\n",sha_fullpath );
					return res;
				}

			}
			else
			{
				//LOGE("DM: No file %s to delete\n", sha_fullpath);

			}
		}
		break;

		case sha_file_get_sha:
		{

			if( !file_exists )
			{
				LOGE("DM: file get sha fail %s\n", original_filename);
				return ~0;
			}

			if(sha)
			{

				uint32_t bytes_to_read=0, bytes_read=0;
				FIL fp = {0};

				//open file for read
				res = hello_fs_open(&fp, sha_fullpath, FA_READ);
				if (res) {
					//LOGE("DM: error opening file for read %d\n", res);
					return res;
				}

				// read into SHA
				bytes_to_read = SHA1_SIZE;
				while (bytes_to_read > 0) {
					res = hello_fs_read(&fp, sha,bytes_to_read, &bytes_read);
					if (res) {
						//LOGE("DM: error reading file %d\n", res);
						return res;
					}
					bytes_to_read -= bytes_read;
				}
				hello_fs_close(&fp);

			}
			else
			{
				LOGE("DM: SHA get, invalid ptr\n");
			}
		}
		break;
	}

	return 0;
}

int cmd_file_sync_upload(int argc, char *argv[])
{

	LOGI("DM: --FILE SYNC UPLOAD!--\n");
	xSemaphoreGive(filemngr_upload_sem);

	return 0;
}

// rw = 0 (write), 1(read)
static uint32_t sd_card_test(bool rw, uint8_t* ptr, filesystem_rw_func_t fs_rw_func)
{

	FRESULT res;
	FIL fp = {0};
	uint32_t bytes_transferred;
	uint8_t open_flags = 0;
	char filename[] = {"/test/test"};
	uint32_t i;

	/* OPEN TEST FILE FOR READ/WRITE */
	open_flags = (rw) ? FA_READ : FA_CREATE_ALWAYS|FA_WRITE;

	res = hello_fs_open(&fp,filename, open_flags);
	if (res) {
		LOGE("DM: test f_open fail %d\n", res);
		return res;
	}

	/* WRITE TEST DATA */
	if(!rw)
	{
		uint32_t* data_ptr = (uint32_t*)ptr;

		for(i=0;i<SD_BLOCK_SIZE/4;i++)
		{
			data_ptr[i] = test_code;

		}
	}

	/* READ/WRITE TEST DATA */
	// Perform a read/write
	res = fs_rw_func(&fp, ptr,SD_BLOCK_SIZE, &bytes_transferred);
	if (res) {
		LOGE("DM: test rw fail %d\n", res);
		return res;
	}

	/* READ TEST DATA */
	if(rw)
	{

		uint32_t* data_ptr = (uint32_t*)ptr;

		for(i=0;i<SD_BLOCK_SIZE/4;i++)
		{
			if(data_ptr[i] != test_code)
			{
				LOGE("DM read at %d: 0x%x\n",i, data_ptr[i]);
				return FR_DISK_ERR;
			}

		}


	}

    // Close file
    hello_fs_close(&fp);
    if(res){
    	return res;
    }

    /* DELETE TEST FILE */
    // Delete file if this is a read
    if(rw)
	{
    	return (hello_fs_unlink(filename));
	}

    return FR_OK;
}

