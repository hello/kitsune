#include "uart_logger.h"
#include "FreeRTOS.h"
#include "event_groups.h"
#include "log.pb.h"
#include <pb_encode.h>
#include "netcfg.h"
#include <stdio.h>
#include <stdbool.h>
#include "networktask.h"
#include "wifi_cmd.h"
#include "uartstdio.h"
#include "debug.h"
#include <stdlib.h>
#include "endpoints.h"
#include "ff.h"
#include "hellofilesystem.h"
#include "fatfs_cmd.h"
#include "proto_utils.h"
#include "ustdlib.h"

#include "kit_assert.h"
#include "sys_time.h"

#define SENSE_LOG_ENDPOINT		"/logs"
#define SENSE_LOG_FOLDER		"logs"
#define SENSE_LOG_RW_SIZE		128
/**
 * The upload order should be
 * Backend(if has no local backlog) -> Local -> Backend(if has IP)
 * This way we can guarantee continuity if wifi is intermittent
 */

//flag to start saving the logs to sd
#define LOG_EVENT_STORE 		0x1
//flag to upload to server and delete oldest file
#define LOG_EVENT_UPLOAD	    0x2
//flag to upload the swap block only
#define LOG_EVENT_UPLOAD_ONLY	0x4
//flag to shutdown
#define LOG_EVENT_EXIT          0x8
//flag to indicate the thread ip up and running
#define LOG_EVENT_READY         0x100


typedef struct {
	char * ptr;
	int pos;
} event_ctx_t;

static struct{
	uint8_t blocks[3][UART_LOGGER_BLOCK_SIZE];
	//ptr to block that is currently used for logging
	uint8_t * logging_block;
	//ptr to block that is being stored to sdcard
	volatile uint8_t * store_block;
	//ptr to block that is used to upload or read from sdcard
	volatile uint8_t * operation_block;
	volatile uint32_t widx;
	EventGroupHandle_t uart_log_events;
	sense_log log;
	uint8_t view_tag;	//what level gets printed out to console
	uint8_t store_tag;	//what level to store to sd card
	xSemaphoreHandle block_sem; //guards logging_block and store_block pointers
	xSemaphoreHandle print_sem; //guards writing to the logging block and widx
	DIR logdir;
	xQueueHandle analytics_event_queue;
}self = {0};

void set_loglevel(uint8_t loglevel) {
	self.store_tag = loglevel;
}

typedef void (file_handler)(FILINFO * info, void * ctx);
static int _walk_log_dir(file_handler * handler, void * ctx);
static FRESULT _remove_oldest(int * rem);

static bool
_encode_text_block(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	return pb_encode_tag(stream, PB_WT_STRING, field->tag)
			&& pb_encode_string(stream, (uint8_t*)self.operation_block,
					UART_LOGGER_BLOCK_SIZE);
}

static void
_swap_and_upload(void){
	if( !self.block_sem ) {
		return;
	}
	xSemaphoreTake(self.block_sem, portMAX_DELAY);
	if (!(xEventGroupGetBitsFromISR(self.uart_log_events) & LOG_EVENT_STORE)) {
		self.store_block = self.logging_block;
		//logc can be called anywhere, so using ISR api instead
		xEventGroupSetBits(self.uart_log_events, LOG_EVENT_STORE);
	} else {
		//operation busy
	}
	//swap
	self.logging_block =
			(self.logging_block == self.blocks[0]) ?
					self.blocks[1] : self.blocks[0];
	//reset
	self.widx = 0;

    xSemaphoreGive( self.block_sem );
}
#ifdef BUILD_SERVERS
void telnetPrint(const char * str, int len );
#endif

static void
_logstr(const char * str, int len, bool echo, bool store){
#ifndef BUILD_SERVERS
	int i;

	if( !self.print_sem ) {
		return;
	}
	xSemaphoreTake(self.print_sem, portMAX_DELAY);
	for(i = 0; i < len && store; i++){
		uart_logc(str[i]);
	}
	xSemaphoreGive(self.print_sem);

#endif

	if (echo) {
#ifdef BUILD_SERVERS
		telnetPrint(str, len);
#endif
		UARTwrite(str, len);
	}
}
static int _walk_log_dir(file_handler * handler, void * ctx){
	FILINFO file_info;
	FRESULT res;
	int fcount = 0;
	res = hello_fs_opendir(&self.logdir,SENSE_LOG_FOLDER);
	if(res != FR_OK){
		return -1;
	}
	for(;;){
		res = hello_fs_readdir(&self.logdir, &file_info);
		if(res != FR_OK){
			fcount = -1;
			break;
		}
		// If the file name is blank, then this is the end of the listing.
		if(!file_info.fname[0]){
			break;
		}
		// If the attribue is directory, then increment the directory count.
		if(!(file_info.fattrib & AM_DIR)){
			fcount++;

			if(handler){
				handler(&file_info, ctx);
			}
		}
	}
	//LOGI("End of log files\r\n");
	return fcount;
}
static void
_find_oldest_log(FILINFO * info, void * ctx){
	//LOGI("log name: %s\r\n",info->fname);
	if(ctx){
		int * counter = (int*)ctx;
		int fcounter = atoi(info->fname);
		if(fcounter <= *counter && *counter > 0){
			*counter = fcounter;
		}else if(*counter < 0){
			*counter = fcounter;
		}
	}
}
static void
_find_newest_log(FILINFO * info, void * ctx){
	//LOGI("log name: %s\r\n",info->fname);
	if (ctx) {
		int * counter = (int*) ctx;
		int fcounter = atoi(info->fname);
		if (fcounter > *counter) {
			*counter = fcounter;
		}
	}
}
static char*
_full_log_name(char * full_name, char * local){
	strcat(full_name, "/");
	strcat(full_name, SENSE_LOG_FOLDER);
	strcat(full_name, "/");
	strcat(full_name, local);
	return full_name;
}
static FRESULT
_open_log(FIL * file, char * local_name, WORD mode){
	char name_buf[32] = {0};
	return hello_fs_open(file, _full_log_name(name_buf, local_name), mode);
}
static FRESULT
_write_file(char * local_name, const char * buffer, WORD size){
	FIL file_obj;
	WORD bytes = 0;
	WORD written = 0;
	FRESULT res = _open_log(&file_obj, local_name, FA_CREATE_NEW|FA_WRITE|FA_OPEN_ALWAYS);
	if(res != FR_OK && res != FR_EXIST){
		//LOGE("File %s open fail, code %d", local_name, res);
		return res;
	}
	do{
		res = hello_fs_write(&file_obj, buffer + written, SENSE_LOG_RW_SIZE, &bytes);
		written += bytes;
	}while(written < size);
	res = hello_fs_close(&file_obj);
	if(res != FR_OK){
		//LOGE("unable to write log file\r\n");
		return res;
	}
	return FR_OK;
}
static FRESULT
_read_file(char * local_name, char * buffer, WORD buffer_size, WORD *size_read){
	FIL file_obj;
	WORD offset = 0;
	WORD read = 0;
	FRESULT res = _open_log(&file_obj, local_name, FA_READ);
	if(res == FR_OK){
		do{
			res = hello_fs_read(&file_obj, (void*)(buffer + offset), SENSE_LOG_RW_SIZE, &read);
			if(res != FR_OK){
				return res;
			}
			offset += read;
		}while(read == SENSE_LOG_RW_SIZE && offset < buffer_size);
		return FR_OK;
	}
	return res;
}
static FRESULT
_remove_file(char * local_name){
	char name_buf[32] = {0};
	return hello_fs_unlink(_full_log_name(name_buf, local_name));
}
static FRESULT
_save_newest(const char * buffer, int size){
	int counter = -1;
	int ret = _walk_log_dir(_find_newest_log, &counter);
	if(ret == 0){
		//LOGI("NO log file exists, creating first log\r\n");
		return _write_file("0", buffer, size);
	}else if(ret > 0 && ret <= UART_LOGGER_FILE_LIMIT && counter >= 0){
		char s[16] = {0};
		usnprintf(s,sizeof(s),"%d",++counter);
		//LOGI("Wr log %d\r\n", counter);
		return _write_file(s, buffer, size);
	}else if(ret > 0 && ret > UART_LOGGER_FILE_LIMIT){
        int rem;
        //LOGW("File size reached, removing oldest\r\n");
        if(FR_OK == _remove_oldest(&rem)){
            char s[16] = {0};
            usnprintf(s,sizeof(s),"%d",++counter);
            //LOGI("Wr log %d\r\n", counter);
            return _write_file(s, buffer, size);
        }else{
            return FR_RW_ERROR;
        }
    }else{
		//LOGW("Write log error: %d \r\n", ret);
	}
	return FR_RW_ERROR;
}
static FRESULT
_read_oldest(char * buffer, int size, WORD * read){
	int counter = -1;
	int ret = _walk_log_dir(_find_oldest_log, &counter);
	if(ret == 0){
		//LOGI("No log file\r\n");
		return FR_NO_FILE;
	}else if(ret > 0 && counter >= 0){
		char s[16] = {0};
		usnprintf(s,sizeof(s),"%d",counter);
		//LOGI("Rd log %d\r\n", counter);
		return _read_file(s,buffer, size, read);
	}
	return FR_RW_ERROR;
}
static FRESULT
_remove_oldest(int * rem){
	int counter = -1;
	int ret = _walk_log_dir(_find_oldest_log, &counter);
	if(ret == 0){
		//LOGI("No log file\r\n");
		return FR_OK;
	} else if (ret > 0 && counter >= 0) {
		char s[16] = { 0 };
		usnprintf(s, sizeof(s), "%d", counter);
		//LOGI("Rm log %d\r\n", counter);
		*rem = (ret - 1);
		return _remove_file(s);
	}
	return FR_RW_ERROR;
}
/**
 * PUBLIC functions
 */
void uart_logger_flush(void){
	//set the task to exit and wait for it to do so
	xEventGroupSetBits(self.uart_log_events, LOG_EVENT_EXIT);
	EventBits_t evnt = xEventGroupWaitBits(
					self.uart_log_events,   /* The event group being tested. */
	                0xff,    /* The bits within the event group to wait for. */
	                pdFALSE,        /* all bits should not be cleared before returning. */
	                pdFALSE,       /* Don't wait for both bits, either bit will do. */
	                portMAX_DELAY );/* Wait for any bit to be set. */

	//write out whatever's left in the logging block
	_save_newest((const char*)self.logging_block, self.widx );

}
int Cmd_log_upload(int argc, char *argv[]){
	_swap_and_upload();
	return 0;
}
void uart_logger_init(void){
	self.store_block = self.blocks[0];
	self.logging_block = self.blocks[1];
	self.operation_block = self.blocks[2];
	self.uart_log_events = xEventGroupCreate();
	xEventGroupClearBits( self.uart_log_events, 0xff );
	xEventGroupSetBits(self.uart_log_events, LOG_EVENT_UPLOAD);
	self.log.text.funcs.encode = _encode_text_block;
	self.log.device_id.funcs.encode = encode_device_id_string;
	self.log.has_unix_time = true;
	self.view_tag = LOG_INFO | LOG_WARNING | LOG_ERROR | LOG_VIEW_ONLY | LOG_FACTORY | LOG_TOP;
	self.store_tag = LOG_INFO | LOG_WARNING | LOG_ERROR | LOG_FACTORY | LOG_TOP;
	vSemaphoreCreateBinary(self.block_sem);
	vSemaphoreCreateBinary(self.print_sem);
	xEventGroupSetBits(self.uart_log_events, LOG_EVENT_READY);
}

extern volatile bool booted;

static const char * const g_pcHex = "0123456789abcdef";

typedef void (*out_func_t)(const char * str, int len, void * data);

void _logstr_wrapper(const char * str, int len, void * data ) {
	uint8_t tag = *(uint8_t*)data;
    bool echo = false;
    bool store = false;
    if(tag & self.view_tag){
    	echo = true;
    }
    if(tag & self.store_tag){
    	store = true;
    }
    if( !booted ) {
    	store = false;
    }

#if UART_LOGGER_PREPEND_TAG > 0
    while(tag){
    	if(tag & LOG_INFO){
    		_logstr("[I]", strlen("[I]"), echo, store);
    		tag &= ~LOG_INFO;
    	}else if(tag & LOG_WARNING){
    		_logstr("[W]", strlen("[W]"), echo, store);
    		tag &= ~LOG_WARNING;
    	}else if(tag & LOG_ERROR){
    		_logstr("[E]", strlen("[E]"), echo, store);
    		tag &= ~LOG_ERROR;
    	}else if(tag & LOG_VIEW_ONLY){
    		tag &= ~LOG_VIEW_ONLY;
    	}else if(tag & LOG_TOP){
    		_logstr("[T]", strlen("[T]"), echo, store);
    		tag &= ~LOG_TOP;
    	}else{
    		tag = 0;
    	}
    }
#endif
    _logstr(str, len, echo, store);
}

void _encode_wrapper(const char * str, int len, void * data ) {
	event_ctx_t * ctx = (event_ctx_t*)data;
    memcpy( ctx->ptr + ctx->pos, str, len );
    ctx->pos+=len;
}

void _va_printf( va_list vaArgP, const char * pcString, out_func_t func, void * data ) {
	unsigned long ulIdx, ulValue, ulPos, ulCount, ulBase, ulNeg;
    char *pcStr, pcBuf[16], cFill;

    while(*pcString)
    {
        for(ulIdx = 0; (pcString[ulIdx] != '%') && (pcString[ulIdx] != '\0');
            ulIdx++)
        {
        }
        func(pcString, ulIdx, data);
        pcString += ulIdx;
        if(*pcString == '%')
        {
            pcString++;
            ulCount = 0;
            cFill = ' ';
again:
            switch(*pcString++)
            {
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                {
                    if((pcString[-1] == '0') && (ulCount == 0))
                    {
                        cFill = '0';
                    }
                    ulCount *= 10;
                    ulCount += pcString[-1] - '0';

                    goto again;
                }
                case 'c':
                {
                    ulValue = va_arg(vaArgP, unsigned long);

                    func((char *)&ulValue, 1, data);

                    break;
                }
                case 'd':
                {
                    ulValue = va_arg(vaArgP, unsigned long);
                    ulPos = 0;
                    if((long)ulValue < 0)
                    {
                        ulValue = -(long)ulValue;
                        ulNeg = 1;
                    }
                    else
                    {
                        ulNeg = 0;
                    }

                    ulBase = 10;
                    goto convert;
                }

                case 's':
                {
                    pcStr = va_arg(vaArgP, char *);
                    for(ulIdx = 0; pcStr[ulIdx] != '\0'; ulIdx++)
                    {
                    }
                    func(pcStr, ulIdx, data);
                    if(ulCount > ulIdx)
                    {
                        ulCount -= ulIdx;
                        while(ulCount--)
                        {
                            func(" ", 1, data);
                        }
                    }
                    break;
                }
                case 'u':
                {
                    ulValue = va_arg(vaArgP, unsigned long);
                    ulPos = 0;
                    ulBase = 10;
                    ulNeg = 0;
                    goto convert;
                }
                case 'x':
                case 'X':
                case 'p':
                {
                    ulValue = va_arg(vaArgP, unsigned long);
                    ulPos = 0;
                    ulBase = 16;
                    ulNeg = 0;
convert:
                    for(ulIdx = 1;
                        (((ulIdx * ulBase) <= ulValue) &&
                         (((ulIdx * ulBase) / ulBase) == ulIdx));
                        ulIdx *= ulBase, ulCount--)
                    {
                    }
                    if(ulNeg)
                    {
                        ulCount--;
                    }

                    if(ulNeg && (cFill == '0'))
                    {

                        pcBuf[ulPos++] = '-';
                        ulNeg = 0;
                    }

                    if((ulCount > 1) && (ulCount < 16))
                    {
                        for(ulCount--; ulCount; ulCount--)
                        {
                            pcBuf[ulPos++] = cFill;
                        }
                    }
                    if(ulNeg)
                    {
                        pcBuf[ulPos++] = '-';
                    }

                    for(; ulIdx; ulIdx /= ulBase)
                    {
                        pcBuf[ulPos++] = g_pcHex[(ulValue / ulIdx) % ulBase];
                    }
                    func(pcBuf, ulPos, data);

                    break;
                }
                case '%':
                {
                    func(pcString - 1, 1, data);

                    break;
                }
                default:
                {
                    break;
                }
            }
        }
    }

}

typedef struct {
	void * buffer;
	void * structdata;
}async_context_t;

static void _free_pb(const NetworkResponse_t * response, char * reply_buf, int reply_sz, void * context){
	async_context_t * ctx = (async_context_t*)context;

	if( !http_response_ok((const char*)reply_buf) ) {
		LOGE("failed up upload analytics\n");
		if (ctx->buffer) {
			LOGE("%s\n", ctx->buffer );
		}
	}

	if (ctx) {
		if (ctx->buffer) {
			vPortFree(ctx->buffer);
		}
		if (ctx->structdata) {
			vPortFree(ctx->structdata);
		}
		vPortFree(ctx);
	}

}
int analytics_event( const char *pcString, ...) {
	//todo make this fail more gracefully if the allocations don't succeed...
	va_list vaArgP;
	event_ctx_t ctx;

	sense_log * log = pvPortMalloc(sizeof(sense_log));
	assert(log);
    memset( log, 0, sizeof(sense_log));

	ctx.pos = 0;
	ctx.ptr = pvPortMalloc(128);
	assert(ctx.ptr);
	memset(ctx.ptr, 0, 128);

    va_start(vaArgP, pcString);
    _va_printf( vaArgP, pcString, _encode_wrapper, &ctx );
    va_end(vaArgP);

    if(self.analytics_event_queue){
    	xQueueSend(self.analytics_event_queue, &ctx, 100);
    }


    return 0;
}

void uart_logc(uint8_t c){

	if (self.widx == UART_LOGGER_BLOCK_SIZE) {
		xSemaphoreGive(self.print_sem);
		_swap_and_upload();
		xSemaphoreTake(self.print_sem, portMAX_DELAY);
	}

	self.logging_block[self.widx] = c;
	self.widx++;

}

static bool send_log() {
	self.log.has_unix_time = false;
#if 0 // if we want this we need to add a file header so stored files will get the origin timestamp and not the uploading timestamp
	if( has_good_time() ) {
		self.log.has_unix_time = true;
		self.log.unix_time = get_time();
	}
#endif
    return NetworkTask_SendProtobuf(true, DATA_SERVER, SENSE_LOG_ENDPOINT,
    		sense_log_fields,&self.log, 0, NULL, NULL);
}
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))  /**< Find the maximum of 2 numbers. */
#endif
void analytics_event_task(void * params){
	int block_len = 0;
	char * block = NULL;
	event_ctx_t evt = {0};
	int32_t time = 0;
	sense_log log = (sense_log){//defaults
		.text.funcs.encode = _encode_string_fields,
		.text.arg = NULL,
		.device_id.funcs.encode = encode_device_id_string,
		.has_unix_time = true,
		.has_property = true,
		.property = LogType_KEY_VALUE,
	};
	self.analytics_event_queue = xQueueCreate(10, sizeof(event_ctx_t));
	assert(self.analytics_event_queue);

	while(1){
		if(pdTRUE == xQueueReceive(self.analytics_event_queue, &evt, 5000)){
			char * next_block = pvPortRealloc(block, max(128, (evt.pos + block_len)));
			//time is set on the first event
			if(!block){
				time = get_time();
				next_block[0] = 0;
			}
			block = next_block;
			if(evt.pos){
				assert(block);
				strcat(block, evt.ptr);
				block_len +=  evt.pos;
				vPortFree(evt.ptr);
			}
		}else if(block != NULL){
			log.text.arg = block;
			log.unix_time = time;
			DISP("Analytics: %s\r\n", block);
		/*	NetworkTask_SendProtobuf(true, DATA_SERVER, SENSE_LOG_ENDPOINT,
					sense_log_fields, &log, 0, _free_pb, NULL);*/

			vPortFree(block);
			block = NULL;
			block_len = 0;

		}else{
			vTaskDelay(1000);
		}
	}
}
void uart_logger_task(void * params){
	uint8_t log_local_enable;
	uart_logger_init();
	hello_fs_mkdir(SENSE_LOG_FOLDER);

	FRESULT res = hello_fs_opendir(&self.logdir,SENSE_LOG_FOLDER);

	if(res != FR_OK){
		//uart logging to sd card is disabled
		log_local_enable = 0;
	}else{
		log_local_enable = 1;
	}
	while(1){

		xEventGroupSetBits(self.uart_log_events, LOG_EVENT_READY);
		EventBits_t evnt = xEventGroupWaitBits(
				self.uart_log_events,   /* The event group being tested. */
                0xff,    /* The bits within the event group to wait for. */
                pdFALSE,        /* all bits should not be cleared before returning. */
                pdFALSE,       /* Don't wait for both bits, either bit will do. */
                portMAX_DELAY );/* Wait for any bit to be set. */
		if( (evnt & LOG_EVENT_EXIT) && !(evnt & LOG_EVENT_STORE)){
			vTaskDelete(0);
			xEventGroupClearBits(self.uart_log_events,LOG_EVENT_EXIT);
			return;
		}
		if( evnt & LOG_EVENT_STORE ) {
			xSemaphoreTake(self.block_sem, portMAX_DELAY);
			if(log_local_enable && FR_OK == _save_newest((char*) self.store_block, UART_LOGGER_BLOCK_SIZE)){
				self.operation_block = self.blocks[2];
				xEventGroupSetBits(self.uart_log_events, LOG_EVENT_UPLOAD);
			}else{
				self.operation_block = self.store_block;
				xEventGroupSetBits(self.uart_log_events, LOG_EVENT_UPLOAD_ONLY);
				//LOGE("Unable to save logs\r\n");
			}
			xEventGroupClearBits(self.uart_log_events, LOG_EVENT_STORE);
			xSemaphoreGive(self.block_sem);
		}
		if( evnt & LOG_EVENT_UPLOAD) {
			xEventGroupClearBits(self.uart_log_events,LOG_EVENT_UPLOAD);
			if(wifi_status_get(HAS_IP)){
				WORD read;
				FRESULT res;
				//operation block is used for file io
				res = _read_oldest((char*)self.operation_block,UART_LOGGER_BLOCK_SIZE, &read);
				if(FR_OK != res){
					LOGE("Unable to read log file %d\r\n",(int)res);
					vTaskDelay(5000);
					continue;
				}

				if(send_log()){
					int rem = -1;
					//LOGI("Log upload succeeded\r\n");
					res = _remove_oldest(&rem);
					if(FR_OK == res && rem > 0){
						xEventGroupSetBits(self.uart_log_events, LOG_EVENT_UPLOAD);
					}else if(FR_OK == res && rem == 0){
						//LOGI("Upload logs done\r\n");
					}else{
						//LOGE("Rm log error %d\r\n", res);
					}
				}else{
					//LOGE("Log upload failed, network code = %d\r\n", ret);
				}
			}
		}
		if(evnt & LOG_EVENT_UPLOAD_ONLY) {
			xEventGroupClearBits(self.uart_log_events,LOG_EVENT_UPLOAD_ONLY);
			if(wifi_status_get(HAS_IP)){
				send_log();
			}
		}
		vTaskDelay(5000);
	}
}

int Cmd_log_setview(int argc, char * argv[]){
    char * pend;
	if(argc > 1){
		self.view_tag = ((uint8_t) strtol(argv[1],&pend,16) )&0xFF;
		return 0;
	}
	return -1;
}


void uart_logf(uint8_t tag, const char *pcString, ...){
	va_list vaArgP;
    va_start(vaArgP, pcString);
    _va_printf( vaArgP, pcString, _logstr_wrapper, &tag );
    va_end(vaArgP);
}
int Cmd_analytics(int argc, char * argv[]){
	if(argc > 2){
		analytics_event("{%s : %s}", argv[1], argv[2]);
	}else{
		DISP("Usage: ana $key $value\r\n");
		return -1;
	}

	return 0;
}
