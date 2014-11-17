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

#define SENSE_LOG_ENDPOINT		"/logs"
#define SENSE_LOG_FOLDER		"logs"

/**
 * The upload order should be
 * Backend(if has no local backlog) -> Local -> Backend(if has IP)
 * This way we can guarantee continuity if wifi is intermittent
 */
#define LOG_EVENT_STORE 		0x1
#define LOG_EVENT_UPLOAD	    0x2
extern unsigned int sl_status;
static struct{
	uint8_t blocks[3][UART_LOGGER_BLOCK_SIZE];
	volatile uint8_t * logging_block;
	volatile uint8_t * upload_block;
	uint8_t * operation_block;
	volatile uint32_t widx;
	EventGroupHandle_t uart_log_events;
	sense_log log;
	uint8_t view_tag;
	uint8_t log_local_enable;
	xSemaphoreHandle block_operation_sem;
	DIR logdir;
}self;

typedef void (file_handler)(FILINFO * info, void * ctx);
static int _walk_log_dir(file_handler * handler, void * ctx);
static bool
_encode_text_block(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	return pb_encode_tag(stream, PB_WT_STRING, field->tag)
			&& pb_encode_string(stream, (uint8_t*)self.operation_block,
					UART_LOGGER_BLOCK_SIZE);
}
static bool
_encode_mac_as_device_id_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    uint8_t mac[6] = {0};
    uint8_t mac_len = 6;
#if FAKE_MAC
    mac[0] = 0xab;
    mac[1] = 0xcd;
    mac[2] = 0xab;
    mac[3] = 0xcd;
    mac[4] = 0xab;
    mac[5] = 0xcd;
#else
    int32_t ret = sl_NetCfgGet(SL_MAC_ADDRESS_GET, NULL, &mac_len, mac);
    if(ret != 0 && ret != SL_ESMALLBUF)
    {
    	UARTprintf("encode_mac_as_device_id_string: Fail to get MAC addr, err %d\n", ret);
        return false;  // If get mac failed, don't encode that field
    }
#endif
    char hex_device_id[13] = {0};
    uint8_t i = 0;
    for(i = 0; i < sizeof(mac); i++){
    	snprintf(&hex_device_id[i * 2], 3, "%02X", mac[i]);
    }
    return pb_encode_tag_for_field(stream, field) && pb_encode_string(stream, (uint8_t*)hex_device_id, strlen(hex_device_id));
}
//
static void
_swap_and_upload(void){
	if (!(xEventGroupGetBitsFromISR(self.uart_log_events) & LOG_EVENT_STORE)) {
		self.upload_block = self.logging_block;
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
}

static void
_logstr(const char * str, int len, bool echo){
	int i;
	for(i = 0; i < len; i++){
		uart_logc(str[i]);
	}
	if(echo){
		UARTwrite(str,len);
	}
}
static int
_walk_log_dir(file_handler * handler, void * ctx){
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
	LOGI("End of log files\r\n");
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
static int
_write_file(char * local_name, const char * buffer, WORD size){
	FIL file_obj;
	WORD bytes = 0;
	WORD written = 0;
	WORD write_size = size;
	FRESULT res = _open_log(&file_obj, local_name, FA_CREATE_NEW|FA_WRITE|FA_OPEN_ALWAYS);
	if(res != FR_OK && res != FR_EXIST){
		LOGE("File %s open fail, code %d", local_name, res);
		return -1;
	}
	do{
		res = hello_fs_write(&file_obj, buffer + written, 128, &bytes);
		written += bytes;
	}while(written < size);
	res = hello_fs_close(&file_obj);
	if(res != FR_OK){
		LOGE("unable to write log file\r\n");
		return -1;
	}
	return 0;
}
static int
_read_file(char * local_name, char * buffer, WORD buffer_size, WORD *size_read){
	FIL file_obj;
	WORD offset = 0;
	WORD read = 0;
	FRESULT res = _open_log(&file_obj, local_name, FA_READ);
	if(res == FR_OK){
		do{
			res = hello_fs_read(&file_obj, (void*)(buffer + offset), 128,
					&read);

			if(res != FR_OK){
				return((int)res);
			}
			offset += read;
		}while(read == 128 && offset < buffer_size);
	}else{
		return (int)res;
	}
}
static int
_remove_file(char * local_name){
	char name_buf[32] = {0};
	return hello_fs_unlink(_full_log_name(name_buf, local_name));
}
static int
_save_newest(const char * buffer, int size){
	int counter = -1;
	int ret = _walk_log_dir(_find_newest_log, &counter);
	if(ret == 0){
		LOGI("NO log file exists, creating first log\r\n");
		return _write_file("0", buffer, size);
	}else if(ret > 0 && counter >= 0){
		char s[16] = {0};
		snprintf(s,sizeof(s),"%d",++counter);
		LOGI("%d log file exists, newest = %d\r\n", ret, counter);
		return _write_file(s, buffer, size);
	}else{
		LOGW("Write log error: %d \r\n", ret);
	}
	return ret;
}
static int
_read_oldest(char * buffer, int size, WORD * read){
	int counter = -1;
	int ret = _walk_log_dir(_find_oldest_log, &counter);
	if(ret == 0){
		LOGI("No log file\r\n");
	}else if(ret > 0 && counter >= 0){
		char s[16] = {0};
		snprintf(s,sizeof(s),"%d",counter);
		LOGI("read log file %d", counter);
		return _read_file(s,buffer, size, read);
	}else{
		LOGW("Read log error %d\r\n", ret);
	}
	return ret;
}
static int
_remove_oldest(int * rem){
	int counter = -1;
	int ret = _walk_log_dir(_find_oldest_log, &counter);
	if(ret == 0){
		LOGI("No log file\r\n");
	} else if (ret > 0 && counter >= 0) {
		char s[16] = { 0 };
		snprintf(s, sizeof(s), "%d", counter);
		LOGI("Removed log file %d", counter);
		*rem = (ret - 1);
		return _remove_file(s);
	} else {
		LOGW("Erase log error %d\r\n", ret);
	}
	return ret;
}
/**
 * PUBLIC functions
 */
int Cmd_log_upload(int argc, char *argv[]){
	_swap_and_upload();
}
void uart_logger_init(void){
	self.upload_block = self.blocks[0];
	self.operation_block = self.blocks[2];
	self.uart_log_events = xEventGroupCreate();
	xEventGroupClearBits( self.uart_log_events, 0xff );
	self.log.text.funcs.encode = _encode_text_block;
	self.log.device_id.funcs.encode = _encode_mac_as_device_id_string;
	self.log.has_unix_time = true;
	self.view_tag = LOG_INFO | LOG_WARNING | LOG_ERROR;
	vSemaphoreCreateBinary(self.block_operation_sem);
}
void uart_logc(uint8_t c){
	//if(xSemaphoreTake(self.block_operation_sem,1)){
		if (self.widx == UART_LOGGER_BLOCK_SIZE) {
			_swap_and_upload();
		}
		self.logging_block[self.widx] = c;
		self.widx++;
		xSemaphoreGive(self.block_operation_sem);
	//}
}

unsigned long get_time();
void uart_logger_task(void * params){
	if(0 != mkdir(SENSE_LOG_FOLDER)){

	}
	FRESULT res = hello_fs_opendir(&self.logdir,SENSE_LOG_FOLDER);
	if(res != FR_OK){
		//uart logging to sd card is disabled
		self.log_local_enable = 0;
	}else{
		self.log_local_enable = 1;
	}
	while(1){
		char buffer[UART_LOGGER_BLOCK_SIZE + UART_LOGGER_RESERVED_SIZE] = {0};
		int ret;
		EventBits_t evnt = xEventGroupWaitBits(
				self.uart_log_events,   /* The event group being tested. */
                0xff,    /* The bits within the event group to wait for. */
                pdFALSE,        /* all bits should not be cleared before returning. */
                pdFALSE,       /* Don't wait for both bits, either bit will do. */
                portMAX_DELAY );/* Wait for any bit to be set. */
		switch(evnt){
		case LOG_EVENT_STORE:
			_save_newest((char*)self.upload_block, UART_LOGGER_BLOCK_SIZE);
			xEventGroupClearBits(self.uart_log_events,LOG_EVENT_STORE);
			xEventGroupSetBits(self.uart_log_events, LOG_EVENT_UPLOAD);
			break;
		case LOG_EVENT_UPLOAD:
			if(sl_status & HAS_IP){
				WORD read;
				self.log.has_unix_time = false;
				//for read oldest block and upload, we are reusing upload_block pointer until more memory is freed
				//so that a upload block can be dedicated to reading old files
				_read_oldest((char*)self.operation_block,UART_LOGGER_BLOCK_SIZE, &read);
				ret = NetworkTask_SynchronousSendProtobuf(DATA_SERVER, SENSE_LOG_ENDPOINT,buffer,sizeof(buffer),sense_log_fields,&self.log,0);
				if(ret == 0){
					int rem = -1;
					LOGI("Log upload succeeded\r\n");
					_remove_oldest(&rem);
					if(rem > 0){
						xEventGroupSetBits(self.uart_log_events, LOG_EVENT_UPLOAD);
					}else{
						xEventGroupClearBits(self.uart_log_events,LOG_EVENT_UPLOAD);
					}
				}else{
					LOGI("Log upload failed\r\n");
				}
			}
			break;
		}
		vTaskDelay(2000);
	}
}
int Cmd_log_setview(int argc, char * argv[]){
	if(argc > 1){
		self.view_tag = ((uint8_t)atoi(argv[1]))&0xF;
		return 0;
	}
	return -1;
}

static const char * const g_pcHex = "0123456789abcdef";
void uart_logf(uint8_t tag, const char *pcString, ...){
	//TODO protect this with sem to prevent chracters inserted inbetween
    unsigned long ulIdx, ulValue, ulPos, ulCount, ulBase, ulNeg;
    char *pcStr, pcBuf[16], cFill;
    bool echo = false;
    va_list vaArgP;
    ASSERT(pcString != 0);
    if(tag & self.view_tag){
    	echo = true;
    }
#if UART_LOGGER_PREPEND_TAG > 0
    while(tag){
    	if(tag & LOG_INFO){
    		_logstr("[INFO]", strlen("[INFO]"), echo);
    		tag &= ~LOG_INFO;
    	}else if(tag & LOG_WARNING){
    		_logstr("[WARNING]", strlen("[WARNING]"), echo);
    		tag &= ~LOG_WARNING;
    	}else if(tag & LOG_ERROR){
    		_logstr("[ERROR]", strlen("[ERROR]"), echo);
    		tag &= ~LOG_ERROR;
    	}else{
    		tag = 0;
    	}
    }
#endif
    va_start(vaArgP, pcString);
    while(*pcString)
    {
        for(ulIdx = 0; (pcString[ulIdx] != '%') && (pcString[ulIdx] != '\0');
            ulIdx++)
        {
        }
        _logstr(pcString, ulIdx, echo);
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

                    _logstr((char *)&ulValue, 1, echo);

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
                    _logstr(pcStr, ulIdx, echo);
                    if(ulCount > ulIdx)
                    {
                        ulCount -= ulIdx;
                        while(ulCount--)
                        {
                            _logstr(" ", 1, echo);
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
                    _logstr(pcBuf, ulPos, echo);

                    break;
                }
                case '%':
                {
                    _logstr(pcString - 1, 1, echo);

                    break;
                }
                default:
                {
                    break;
                }
            }
        }
    }

    va_end(vaArgP);
	_logstr("\r\n", strlen("\r\n"), echo);
}
