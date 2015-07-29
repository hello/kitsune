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
#include "hlo_queue.h"

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
	uint8_t * logging_block; //used for logging uart text
	hlo_queue_t * logging_queue;
	//ptr to block that is used to upload or read from sdcard
	uint32_t widx;
	sense_log log;		//cached protobuf datastructure
	uint8_t view_tag;	//what level gets printed out to console
	uint8_t store_tag;	//what level to store to sd card
	xSemaphoreHandle print_sem; //guards writing to the logging block and widx
	xQueueHandle analytics_event_queue;

}self;

void set_loglevel(uint8_t loglevel) {
	self.store_tag = loglevel;
}

static bool
_encode_text_block(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	return pb_encode_tag(stream, PB_WT_STRING, field->tag)
			&& pb_encode_string(stream, (const char *)arg,
					UART_LOGGER_BLOCK_SIZE);
}

#ifdef BUILD_SERVERS
void telnetPrint(const char * str, int len );
#endif
void on_write_flash_finished(void * buf){
	DISP("Freed\r\n");
	vPortFree(buf);
}
static void _queue_and_reset_block(void){
	hlo_queue_enqueue(self.logging_queue, self.logging_block, UART_LOGGER_BLOCK_SIZE, 0,on_write_flash_finished);

	self.logging_block =  pvPortMalloc(UART_LOGGER_BLOCK_SIZE);
	assert(self.logging_block);
	memset(self.logging_block, 0, UART_LOGGER_BLOCK_SIZE);
	self.widx = 0;
}

static void
_logstr(const char * str, int len, bool echo, bool store){
#ifndef BUILD_SERVERS
	int i;

	if( !self.print_sem ) {
		return;
	}
	xSemaphoreTakeRecursive(self.print_sem, portMAX_DELAY);
	if( self.widx + len >= UART_LOGGER_BLOCK_SIZE) {
		_queue_and_reset_block();
	}
	for(i = 0; i < len && store; i++){
		uart_logc(str[i]);
	}
	xSemaphoreGiveRecursive(self.print_sem);

#endif

	if (echo) {
#ifdef BUILD_SERVERS
		telnetPrint(str, len);
#endif
		UARTwrite(str, len);
	}
}

/**
 * PUBLIC functions
 */
void uart_logger_flush(void){
	//set the task to exit and wait for it to do so
	//_save_block_queue(0);
	//write out whatever's left in the logging block
	//_save_newest((const char*)self.logging_block, self.widx );
}
int Cmd_log_upload(int argc, char *argv[]){
	vTaskDelay(1000);
	xSemaphoreTakeRecursive(self.print_sem, portMAX_DELAY);
	_queue_and_reset_block();
	xSemaphoreGiveRecursive(self.print_sem);
	return 0;
}
void uart_logger_init(void){
	self.logging_block = NULL;
	self.logging_queue = hlo_queue_create(SENSE_LOG_FOLDER,64,0);
	assert(self.logging_queue);

	self.log.text.funcs.encode = _encode_text_block;
	self.log.device_id.funcs.encode = encode_device_id_string;
	self.log.has_unix_time = true;

	self.view_tag = LOG_INFO | LOG_WARNING | LOG_ERROR | LOG_VIEW_ONLY | LOG_FACTORY | LOG_TOP;
	self.store_tag = LOG_INFO | LOG_WARNING | LOG_ERROR | LOG_FACTORY | LOG_TOP;

	self.print_sem = xSemaphoreCreateRecursiveMutex();

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

    return 0;
}

void uart_logc(uint8_t c){
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
	//no timeout on this one...
    return NetworkTask_SendProtobuf(true, DATA_SERVER, SENSE_LOG_ENDPOINT,
    		sense_log_fields,&self.log, 0, NULL, NULL, NULL, false);
}

void analytics_event_task(void * params){
	int block_len = 0;
	char * block = pvPortMalloc(ANALYTICS_MAX_CHUNK_SIZE);
	assert(block);
	memset(block, 0, ANALYTICS_MAX_CHUNK_SIZE);
	event_ctx_t evt = {0};
	int32_t time = 0;
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
			portTickType now = xTaskGetTickCount();
			DISP("Analytics: %s\r\n", block);
			if( !NetworkTask_SendProtobuf(true, DATA_SERVER, SENSE_LOG_ENDPOINT,
					sense_log_fields, &log, 0, NULL, NULL, NULL, false) ) {
				LOGI("Analytics failed to upload\n");
			}
			block_len = 0;
			memset(block, 0, ANALYTICS_MAX_CHUNK_SIZE);
			vTaskDelayUntil(&now, 1000);
		}
	}
}

void uart_logger_task(void * params){
	uart_logger_init();
	while(1){
		void * out_buf = NULL;
		size_t out_size = 0;
		if(hlo_queue_dequeue(self.logging_queue, &out_buf, &out_size) >= 0){
			if(out_buf && out_size){
				DISP("uploading log %d bytes\r\n", out_size);
				vPortFree(out_buf);
			}
		}
		vTaskDelay(1000);
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
