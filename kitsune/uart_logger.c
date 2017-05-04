#include "uart_logger.h"

#include "FreeRTOS.h"
#include "event_groups.h"
#include "queue.h"

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

#include "limits.h"

#define SENSE_LOG_ENDPOINT		"/logs"
#define SENSE_LOG_FOLDER		"logs"
/**
 * The upload order should be
 * Backend(if has no local backlog) -> Local -> Backend(if has IP)
 * This way we can guarantee continuity if wifi is intermittent
 */

//flag to upload to server and delete oldest file
#define LOG_EVENT_UPLOAD	    0x2
//flag to upload the swap block only
#define LOG_EVENT_UPLOAD_ONLY	0x4
//flag to indicate the thread ip up and running
#define LOG_EVENT_READY         0x100


typedef struct {
	char * ptr;
	int pos;
} event_ctx_t;

static struct{
	xQueueHandle block_queue;
	uint8_t * logging_block;
	//ptr to block that is used to upload or read from sdcard
	uint8_t operation_block[UART_LOGGER_BLOCK_SIZE];
	uint32_t widx;
	EventGroupHandle_t uart_log_events;
	sense_log log;
	uint16_t view_tag;	//what level gets printed out to console
	uint16_t store_tag;	//what level to store to sd card
	xSemaphoreHandle print_sem; //guards writing to the logging block and widx
	DIR logdir;
	xQueueHandle analytics_event_queue;
}self = {0};

void set_loglevel(uint16_t loglevel) {
	self.store_tag = loglevel;
}

void add_loglevel(uint16_t loglevel ) {
	self.store_tag |= loglevel;
	self.view_tag |= loglevel;
}
void rem_loglevel(uint16_t loglevel ) {
	self.store_tag &= ~loglevel;
	self.view_tag &= ~loglevel;
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
	if( !xQueueSend(self.block_queue, (void* )&self.logging_block, 0) ) {
    	vPortFree(self.logging_block);
    }
	//swap
	self.logging_block = NULL;
	//reset
	self.widx = 0;
}
#ifdef BUILD_TELNET_SERVER
void telnetPrint(const char * str, int len );
#endif

static void
_logstr(const char * str, int len, bool echo, bool store){
#ifndef BUILD_TELNET_SERVER
	int i;

	if( !self.print_sem ) {
		return;
	}
	xSemaphoreTake(self.print_sem, portMAX_DELAY);
	if( store ) {
		if( self.widx + len >= UART_LOGGER_BLOCK_SIZE) {
			_swap_and_upload();
		}
		if( !self.logging_block ) {
			self.logging_block = pvPortMalloc(UART_LOGGER_BLOCK_SIZE);
			assert(self.logging_block);
			memset( (void*)self.logging_block, 0, UART_LOGGER_BLOCK_SIZE );
		}
		for(i = 0; i < len; i++){
			assert( self.widx < UART_LOGGER_BLOCK_SIZE );
			self.logging_block[self.widx++] = str[i];
		}
	}

#endif

	if (echo) {
#ifdef BUILD_TELNET_SERVER
		telnetPrint(str, len);
#endif
		UARTwrite(str, len);
	}
	xSemaphoreGive(self.print_sem);
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
	DISP("End of log files\r\n");
	return fcount;
}
static void
_find_oldest_log(FILINFO * info, void * ctx){
	//DISP("log name: %s\r\n",info->fname);
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
	//DISP("log name: %s\r\n",info->fname);
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
	usnprintf(full_name, 32, "/" SENSE_LOG_FOLDER "/%s", local);
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
	UINT bytes = 0;
	WORD written = 0;
	FRESULT res = _open_log(&file_obj, local_name, FA_CREATE_NEW|FA_WRITE|FA_OPEN_ALWAYS);
	if(res != FR_OK && res != FR_EXIST){
		LOGE("File %s open fail, code %d", local_name, res);
		return res;
	}
	do{
		res = hello_fs_write(&file_obj, buffer + written, SENSE_LOG_RW_SIZE, &bytes);
		written += bytes;
	}while(written < size);
	res = hello_fs_close(&file_obj);
	if(res != FR_OK){
		LOGE("unable to write log file\r\n");
		return res;
	}
	return FR_OK;
}

static FRESULT
_read_file(char * local_name, char * buffer, WORD buffer_size, WORD *size_read){
	FIL file_obj;
	WORD offset = 0;
	UINT read = 0;
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
		DISP("NO log file exists, creating first log\r\n");
		return _write_file("0", buffer, size);
	}else if(ret > 0 && ret <= UART_LOGGER_FILE_LIMIT && counter >= 0){
		char s[16] = {0};
		usnprintf(s,sizeof(s),"%d",++counter);
		DISP("Wr log %d\r\n", counter);
		return _write_file(s, buffer, size);
	}else if(ret > 0 && ret > UART_LOGGER_FILE_LIMIT){
        int rem;
        //LOGW("File size reached, removing oldest\r\n");
        if(FR_OK == _remove_oldest(&rem)){
            char s[16] = {0};
            usnprintf(s,sizeof(s),"%d",++counter);
            DISP("Wr log %d\r\n", counter);
            return _write_file(s, buffer, size);
        }else{
            return FR_DISK_ERR;
        }
    }else{
		//LOGW("Write log error: %d \r\n", ret);
	}
	return FR_DISK_ERR;
}
static FRESULT
_read_oldest(char * buffer, int size, WORD * read){
	int counter = -1;
	int ret = _walk_log_dir(_find_oldest_log, &counter);
	if(ret == 0){
		DISP("No log file\r\n");
		return FR_NO_FILE;
	}else if(ret > 0 && counter >= 0){
		char s[16] = {0};
		usnprintf(s,sizeof(s),"%d",counter);
		DISP("Rd log %d\r\n", counter);
		return _read_file(s,buffer, size, read);
	}
	return FR_DISK_ERR;
}
static FRESULT
_remove_oldest(int * rem){
	int counter = -1;
	int ret = _walk_log_dir(_find_oldest_log, &counter);
	if(ret == 0){
		DISP("No log file\r\n");
		return FR_OK;
	} else if (ret > 0 && counter >= 0) {
		char s[16] = { 0 };
		usnprintf(s, sizeof(s), "%d", counter);
		DISP("Rm log %d\r\n", counter);
		*rem = (ret - 1);
		return _remove_file(s);
	}
	return FR_DISK_ERR;
}
static uint8_t log_local_enable;
static void _save_block_queue( TickType_t dly ) {
	uint8_t * store_block;
	while( xQueueReceive(self.block_queue, &store_block, dly ) ) {
		if(log_local_enable && FR_OK == _save_newest((char*)store_block, UART_LOGGER_BLOCK_SIZE)){
			xEventGroupSetBits(self.uart_log_events, LOG_EVENT_UPLOAD);
		}else{
			memcpy( self.operation_block, store_block, UART_LOGGER_BLOCK_SIZE );
			xEventGroupSetBits(self.uart_log_events, LOG_EVENT_UPLOAD_ONLY);
			LOGE("Unable to save logs\r\n");
		}
		vPortFree(store_block);
		store_block = NULL;
	}
}
/**
 * PUBLIC functions
 */
void uart_logger_flush_err_shutdown(void){
	//set the task to exit and wait for it to do so
	//xSemaphoreTake(self.print_sem, portMAX_DELAY);
	self.view_tag = self.store_tag = 0;
	//xSemaphoreGive(self.print_sem);
	//write out whatever's left in the logging block
	_save_newest((const char*)self.logging_block, self.widx );
	_save_block_queue(0);
}
int Cmd_log_upload(int argc, char *argv[]){
	xSemaphoreTake(self.print_sem, portMAX_DELAY);
	_swap_and_upload();
	xSemaphoreGive(self.print_sem);
	return 0;
}
void uart_logger_init(void){
	self.block_queue = NULL;
	self.logging_block = NULL;
	self.uart_log_events = xEventGroupCreate();
	xEventGroupClearBits( self.uart_log_events, 0xff );
	xEventGroupSetBits(self.uart_log_events, LOG_EVENT_UPLOAD);
	self.log.text.funcs.encode = _encode_text_block;
	self.log.device_id.funcs.encode = encode_device_id_string;
	self.log.has_unix_time = true;
	self.view_tag = LOG_INFO | LOG_WARNING | LOG_ERROR | LOG_VIEW_ONLY | LOG_FACTORY | LOG_TOP;
	self.store_tag = LOG_INFO | LOG_WARNING | LOG_ERROR | LOG_FACTORY | LOG_TOP;

	self.block_queue = xQueueCreate(2, sizeof(uint8_t*));

	vSemaphoreCreateBinary(self.print_sem);
	xEventGroupSetBits(self.uart_log_events, LOG_EVENT_READY);
}

extern volatile bool booted;

static const char * const g_pcHex = "0123456789abcdef";

typedef void (*out_func_t)(const char * str, int len, void * data);

void _logstr_wrapper(const char * str, int len, void * data ) {
	uint16_t tag = *(uint16_t*)data;
    bool echo = false;
    bool store = false;
    if(tag & self.view_tag){
    	echo = true;
    }
#ifdef SENSE_LTS
    if(tag & self.store_tag){
    	store = true;
    }
#endif
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

int analytics_event( const char *pcString, ...) {
	//todo make this fail more gracefully if the allocations don't succeed...
	va_list vaArgP;
	event_ctx_t ctx = {0};

#ifndef SENSE_LTS
	ctx.ptr = pvPortMalloc(128);
	assert(ctx.ptr);
	memset(ctx.ptr, 0, 128);

    va_start(vaArgP, pcString);
    _va_printf( vaArgP, pcString, _encode_wrapper, &ctx );
    va_end(vaArgP);

    if(self.analytics_event_queue){
    	if( !xQueueSend(self.analytics_event_queue, &ctx, 100) ) {
    		vPortFree(ctx.ptr);
    	}
    }
#endif

    return 0;
}

static bool send_log() {
	self.log.has_unix_time = false;
#if 0 // if we want this we need to add a file header so stored files will get the origin timestamp and not the uploading timestamp
	if( has_good_time() ) {
		self.log.has_unix_time = true;
		self.log.unix_time = get_time();
	}
#endif
	//no timeout on this one...
#if 1
    return NetworkTask_SendProtobuf(true, DATA_SERVER, SENSE_LOG_ENDPOINT,
    		sense_log_fields,&self.log, 0, NULL, NULL, NULL, false);
#else
    return (bool)1;
#endif
}

void analytics_event_task(void * params){
	int block_len = 0;
	char * block = pvPortMalloc(ANALYTICS_MAX_CHUNK_SIZE);
	assert(block);
	memset(block, 0, ANALYTICS_MAX_CHUNK_SIZE);
	event_ctx_t evt = {0};
	uint32_t time = 0;
	sense_log log = (sense_log){//defaults
		.text.funcs.encode = _encode_string_fields,
		.text.arg = block,
		.device_id.funcs.encode = encode_device_id_string,
		.has_unix_time = true,
		.has_property = true,
		.property = LogType_KEY_VALUE,
	};
	self.analytics_event_queue = xQueueCreate(10, sizeof(event_ctx_t));
	assert(self.analytics_event_queue);

	while(1){
		portTickType now = xTaskGetTickCount();
		if(pdTRUE == xQueueReceive(self.analytics_event_queue, &evt, ANALYTICS_WAIT_TIME)){
			int fit_size = evt.pos + block_len;
			DISP("Fit to %d Bytes\r\n", fit_size);

			if(fit_size >= ANALYTICS_MAX_CHUNK_SIZE){
				xQueueSendToFront(self.analytics_event_queue, &evt, 100);
				goto upload;
			}

			if(block_len == 0){			//time is set on the first event
				time = get_time();
			}

			strcat(block, evt.ptr);
			block_len +=  evt.pos;
			vPortFree(evt.ptr);
		}else if(block_len != 0){
upload:
			log.unix_time = time;
			DISP("Analytics: %s\r\n", block);
#if 1
			if( !NetworkTask_SendProtobuf(true, DATA_SERVER, SENSE_LOG_ENDPOINT,
					sense_log_fields, &log, 0, NULL, NULL, NULL, false) ) {
				LOGI("Analytics failed to upload\n");
			}
#else
#endif
			block_len = 0;
			memset(block, 0, ANALYTICS_MAX_CHUNK_SIZE);
		}
		vTaskDelayUntil(&now, 1000);
	}
}
void uart_block_saver_task(void* params) {
	_save_block_queue(portMAX_DELAY);
}
void uart_logger_task(void * params){
	hello_fs_mkdir(SENSE_LOG_FOLDER);

	FRESULT res = hello_fs_opendir(&self.logdir,SENSE_LOG_FOLDER);

	if(res != FR_OK){
		//uart logging to sd card is disabled
		log_local_enable = 0;
	}else{
		log_local_enable = 1;
	}

	xTaskCreate(uart_block_saver_task, "log saver task",   UART_LOGGER_THREAD_STACK_SIZE / 4 , NULL, 2, NULL);

	while(1){
		xEventGroupSetBits(self.uart_log_events, LOG_EVENT_READY);
		EventBits_t evnt = xEventGroupWaitBits(
				self.uart_log_events,   /* The event group being tested. */
                0xff,    /* The bits within the event group to wait for. */
                pdFALSE,        /* all bits should not be cleared before returning. */
                pdFALSE,       /* Don't wait for both bits, either bit will do. */
                portMAX_DELAY );/* Wait for any bit to be set. */
		if( evnt & LOG_EVENT_UPLOAD) {
			xEventGroupClearBits(self.uart_log_events,LOG_EVENT_UPLOAD);
			if(wifi_status_get(UPLOADING)){
				WORD read;
				FRESULT res;
				//operation block is used for file io
				res = _read_oldest((char*)self.operation_block,UART_LOGGER_BLOCK_SIZE, &read);
				if(FR_OK != res){
					LOGE("Unable to read log file %d\r\n",(int)res);
					vTaskDelay(5000);
					continue;
				}
				while(!send_log()){
					LOGE("Log upload failed\r\n");
					vTaskDelay(10000);
				}
				{
					int rem = -1;
					res = _remove_oldest(&rem);
					if(FR_OK == res && rem > 0){
						xEventGroupSetBits(self.uart_log_events, LOG_EVENT_UPLOAD);
					}else if(FR_OK == res && rem == 0){
						//DISP("Upload logs done\r\n");
					}else{
						LOGE("Rm log error %d\r\n", res);
					}
				}
			}
		}
		if(evnt & LOG_EVENT_UPLOAD_ONLY) {
			xEventGroupClearBits(self.uart_log_events,LOG_EVENT_UPLOAD_ONLY);
			if(wifi_status_get(UPLOADING)){
				send_log();
			}
		}
		vTaskDelay(5000);
	}
}

int Cmd_log_setview(int argc, char * argv[]){
    char * pend;
	if(argc > 1){
		self.view_tag = ((uint16_t) strtol(argv[1],&pend,16) )&0xFFFF;
		return 0;
	}
	return -1;
}


void uart_logf(uint16_t tag, const char *pcString, ...){
	va_list vaArgP;

    if(!(tag & (self.view_tag|self.store_tag))) return;
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
static int uart_write(void * ctx, const void * buf, size_t size){
	int i = 0;
	if( xSemaphoreTake(self.print_sem, 0) != pdPASS ) {
			return 0;
	}
	while(UARTTxBytesFree() > 1 && i < size){
		//\n gets turned into \r\n so we need 2 minimum
		UARTwrite((char*)buf+i, 1);
		i++;
	}
	xSemaphoreGive( self.print_sem );
	return i;
}
#undef write
static hlo_stream_vftbl_t uart_stream_impl = {
		.write = uart_write,
};
hlo_stream_t * uart_stream(void){
	static hlo_stream_t * stream;
	if(!stream){
		stream = hlo_stream_new(&uart_stream_impl,NULL,HLO_STREAM_READ_WRITE);
	}
	return stream;
}
