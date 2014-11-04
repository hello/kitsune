#include "uart_logger.h"
#include "FreeRTOS.h"
#include "event_groups.h"
#include "SenseLog.pb.h"
#include <pb_encode.h>
#include "netcfg.h"
#include <stdio.h>
#include <stdbool.h>
#include "networktask.h"
#include "wifi_cmd.h"
#include "uartstdio.h"

#define SENSE_LOG_ENDPOINT		"/logs"
#define LOG_EVENT_BACKEND 		0x1
#define LOG_EVENT_LOCAL			0x2
#define LOG_EVENT_UPLOAD_LOCAL 	0x4
extern unsigned int sl_status;
static struct{
	uint8_t blocks[2][UART_LOGGER_BLOCK_SIZE];
	volatile uint8_t * logging_block;
	volatile uint8_t * upload_block;
	volatile uint32_t widx;
	EventGroupHandle_t uart_log_events;
	SenseLog log;
	uint8_t view_tag;
}self;
static const char * const g_pcHex = "0123456789abcdef";
static bool _encode_text_block(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	return pb_encode_tag(stream, PB_WT_STRING, field->tag)
			&& pb_encode_string(stream, (uint8_t*)self.upload_block,
					UART_LOGGER_BLOCK_SIZE);
}
static bool encode_mac_as_device_id_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
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
static void _swap_and_upload(void){
	if (!(xEventGroupGetBitsFromISR(self.uart_log_events) & LOG_EVENT_BACKEND)) {
		self.upload_block = self.logging_block;
		//logc can be called anywhere, so using ISR api instead
		xEventGroupSetBits(self.uart_log_events, LOG_EVENT_BACKEND);
	} else if (!(xEventGroupGetBitsFromISR(self.uart_log_events)
			& LOG_EVENT_LOCAL)) {
		//TODO write speed is faster than upload speed, delegate the block to SD flash instead
		xEventGroupSetBits(self.uart_log_events, LOG_EVENT_LOCAL);
	} else {
		//both sd card and internet are busy, wtfmate
	}
	//swap
	self.logging_block =
			(self.logging_block == self.blocks[0]) ?
					self.blocks[1] : self.blocks[0];
	//reset
	self.widx = 0;
}

void _logstr(const char * str, int len, bool echo){
	int i;
	for(i = 0; i < len; i++){
		uart_logc(str[i]);
	}
	if(echo){
		UARTwrite(str,len);
	}
}


int Cmd_log_upload(int argc, char *argv[]){
	_swap_and_upload();
	return 0;
}
void uart_logger_init(void){
	self.upload_block = self.blocks[0];
	self.uart_log_events = xEventGroupCreate();
	xEventGroupClearBits( self.uart_log_events, 0xff );
	self.log.text.funcs.encode = _encode_text_block;
	self.log.device_id.funcs.encode = encode_mac_as_device_id_string;
	self.log.has_unix_time = true;
	self.view_tag = LOG_INFO | LOG_WARNING | LOG_ERROR;
}
void uart_logc(uint8_t c){
	if (self.widx == UART_LOGGER_BLOCK_SIZE) {
		_swap_and_upload();
	}
	self.logging_block[self.widx] = c;
	self.widx++;
}
unsigned long get_time();
void uart_logger_task(void * params){
	while(1){
		char buffer[UART_LOGGER_BLOCK_SIZE + 64] = {0};
		int ret;
		EventBits_t evnt = xEventGroupWaitBits(
				self.uart_log_events,   /* The event group being tested. */
                0xff,    /* The bits within the event group to wait for. */
                pdFALSE,        /* all bits should not be cleared before returning. */
                pdFALSE,       /* Don't wait for both bits, either bit will do. */
                portMAX_DELAY );/* Wait for any bit to be set. */
		switch(evnt){
		case LOG_EVENT_BACKEND:
			UARTprintf("Uploading UART logs to server...");
			if(sl_status & HAS_IP){
				self.log.has_unix_time = true;
				self.log.unix_time = get_time();
			}else{
				self.log.has_unix_time = false;
			}

			ret = NetworkTask_SynchronousSendProtobuf(SENSE_LOG_ENDPOINT,buffer,sizeof(buffer),SenseLog_fields,&self.log,0);
			if(ret == 0){
				UARTprintf("Succeeded\r\n");
			}else{
				UARTprintf("Failed\r\n");
				//TODO, failed tx, logging local
			}
			xEventGroupClearBits(self.uart_log_events,LOG_EVENT_BACKEND);
			break;
		case LOG_EVENT_LOCAL:
			UARTprintf("Logging to SD Card\r\n");
			xEventGroupClearBits(self.uart_log_events,LOG_EVENT_LOCAL);
			break;
		}
	}
}
int Cmd_log_setview(int argc, char * argv[]){
	if(argc > 1){
		self.view_tag = ((uint8_t)atoi(argv[1]))&0xF;
		return 0;
	}
	return -1;
}
void uart_logf(uint8_t tag, const char *pcString, ...){
    unsigned long ulIdx, ulValue, ulPos, ulCount, ulBase, ulNeg;
    char *pcStr, pcBuf[16], cFill;
    bool echo = false;
    va_list vaArgP;
    ASSERT(pcString != 0);
    if(tag & self.view_tag){
    	echo = true;
    }
    while(tag){
		 switch(tag){
		case LOG_INFO:
		_logstr("[INFO]", strlen("[INFO]"), echo);
		tag &= ~LOG_INFO;
		break;
		case LOG_WARNING:
		_logstr("[WARNING]", strlen("[WARNING]"), echo);
		tag &= ~LOG_WARNING;
		break;
		case LOG_ERROR:
		_logstr("[ERROR]", strlen("[ERROR]"), echo);
		tag &= ~LOG_ERROR;
		break;
		default:
		tag = 0;
		}
    }

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
