#include "uart_logger.h"
#include "FreeRTOS.h"
#include "event_groups.h"

#define LOG_EVENT_BACKEND 	0x1
#define LOG_EVENT_LOCAL		0x2
static struct{
	uint8_t blocks[2][UART_LOGGER_BLOCK_SIZE];
	uint8_t * logging_block;
	uint8_t * upload_block;
	uint32_t widx;
	EventGroupHandle_t uart_log_events;
}self;

void uart_logger_init(void){
	self.uart_log_events = xEventGroupCreate();
	xEventGroupClearBits( self.uart_log_events, 0xff );
}
void uart_logc(uint8_t c){
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	if (self.widx == UART_LOGGER_BLOCK_SIZE) {
		if(!(xEventGroupGetBitsFromISR(self.uart_log_events) & LOG_EVENT_BACKEND)){
			self.upload_block = self.logging_block;
			//logc can be called anywhere, so using ISR api instead
			xEventGroupSetBitsFromISR(self.uart_log_events, LOG_EVENT_BACKEND, &xHigherPriorityTaskWoken);
		}else{
			//TODO write speed is faster than upload speed, delegate the block to SD flash instead
			xEventGroupSetBitsFromISR(self.uart_log_events, LOG_EVENT_LOCAL,&xHigherPriorityTaskWoken);
		}
		//swap
		self.logging_block = (self.logging_block == self.blocks[0])?self.blocks[1]:self.blocks[0];
		//reset
		self.widx = 0;
	}
	self.logging_block[self.widx] = c;
	self.widx++;

}
void uart_logger_task(void * params){
	while(1){
		EventBits_t evnt = xEventGroupWaitBits(
				self.uart_log_events,   /* The event group being tested. */
                0xff,    /* The bits within the event group to wait for. */
                pdFALSE,        /* all bits should not be cleared before returning. */
                pdFALSE,       /* Don't wait for both bits, either bit will do. */
                portMAX_DELAY );/* Wait for any bit to be set. */
		switch(evnt){
		case LOG_EVENT_BACKEND:
			//NetworkTask_SynchronousSendProtobuf(DATA_RECEIVE_ENDPOINT,buffer,sizeof(buffer),periodic_data_fields,&msg,MAX_RETRY_TIME_IN_TICKS_PERIODIC_DATA);
			break;
		case LOG_EVENT_LOCAL:
			break;
		}
	}
}
