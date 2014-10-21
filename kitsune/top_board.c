#include "top_board.h"
#include "top_hci.h"
#include "slip_packet.h"
#include "rom_map.h"
#include <hw_types.h>
#include <uart.h>
#include <hw_memmap.h>
#include <prcm.h>
#include <stdint.h>
#include <string.h>

typedef enum {
	DFU_INVALID_PACKET = 0,
	DFU_INIT_PACKET = 1,
	DFU_START_DATA_PACKET = 2,
	DFU_DATA_PACKET = 3,
	DFU_STOP_DATA_PACKET = 4
} dfu_packet_type;

static struct{
	enum{
		TOP_NORMAL_MODE = 0,
		TOP_DFU_MODE
	}mode;
	dfu_packet_type dfu_state;
}self;
//test
static uint8_t test_bin[256];
static uint32_t write_idx;
//test end
static void
_printchar(uint8_t c){
	UARTCharPutNonBlocking(UARTA0_BASE, c); //basic feedback
}

static int32_t
_encode_and_send(uint8_t* orig, uint32_t size){
	uint32_t out_size, ret;
	uint8_t * msg = hci_encode(orig, size, &out_size);
	if(msg){
		ret = slip_write(msg,out_size);
		hci_free(msg);
		return ret;
	}
	return -1;
}
static dfu_packet_type
_next_data_block(uint8_t * write_buf, uint32_t buffer_size, uint32_t * out_actual_size){
	if(write_idx + buffer_size >= sizeof(test_bin)){
		memcpy(write_buf, test_bin+write_idx, sizeof(test_bin) - write_idx);
		*out_actual_size = sizeof(test_bin) - write_idx;
		return DFU_STOP_DATA_PACKET;
	}else{
		memcpy(write_buf, test_bin + write_idx, buffer_size);
		write_idx += buffer_size;
		*out_actual_size = buffer_size;
		return DFU_DATA_PACKET;
	}
}
static void
_on_slip_message(uint8_t * c, uint32_t size){

	char * msg;
	uint32_t err = hci_decode(c, size, NULL);


	//move below to ack
	vTaskDelay(100);
	switch (self.dfu_state) {
	case DFU_START_DATA_PACKET:
	{
		self.dfu_state = DFU_INIT_PACKET;
		uint32_t init_packet[] = {(uint32_t)DFU_INIT_PACKET, (uint32_t)hci_crc16_compute(test_bin,sizeof(test_bin))};
		UARTprintf("crc = %x\r\n", hci_crc16_compute(test_bin, sizeof(test_bin)));
		_encode_and_send(init_packet,sizeof(init_packet));

	}
		break;
	case DFU_INIT_PACKET:
	case DFU_DATA_PACKET:
	{
		uint32_t block[32];
		uint32_t written;
		block[0] = DFU_DATA_PACKET;
		self.dfu_state = _next_data_block( ((uint8_t*)block)+sizeof(uint32_t), (sizeof(block) - sizeof(uint32_t)), &written);
		_encode_and_send(block,written + sizeof(block[0]));
	}
		break;
	case DFU_STOP_DATA_PACKET:
	{
		uint32_t end_packet[] = {DFU_STOP_DATA_PACKET};
		_encode_and_send(end_packet, sizeof(end_packet));
		self.dfu_state = DFU_INVALID_PACKET;
	}
		break;
	}

}

static void
_sendchar(uint8_t c){
	UARTCharPut(UARTA1_BASE, c);
}

int top_board_task(void){
	slip_handler_t me = {
			.slip_display_char = _printchar,
			.slip_on_message = _on_slip_message,
			.slip_put_char = _sendchar
	};
	slip_reset(&me);
	hci_init();
	MAP_UARTConfigSetExpClk(UARTA1_BASE, PRCMPeripheralClockGet(PRCM_UARTA1),
			38400,
			(UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
	while (1) {
		uint8_t c = UARTCharGet(UARTA1_BASE);
		slip_handle_rx(c);
	}
	return 1;
}

int top_board_dfu_begin(void){
	uint32_t i;
	for(i = 0; i < sizeof(test_bin); i++){
		test_bin[i] = i & 0xFFu;
	}
	self.dfu_state = DFU_START_DATA_PACKET;
	uint32_t begin_packet[] = {DFU_START_DATA_PACKET, sizeof(test_bin)};
	_encode_and_send((uint8_t*)begin_packet, sizeof(begin_packet));

}

int Cmd_send_top(int argc, char *argv[]){
	int i;
	if(self.mode == TOP_NORMAL_MODE){
		for (i = 1; i < argc; i++) {
			int j = 0;
			while (argv[i][j] != '\0') {
				UARTCharPut(UARTA1_BASE, argv[i][j]);
				j++;
			}
			UARTCharPut(UARTA1_BASE, ' ');
		}
		UARTCharPut(UARTA1_BASE, '\r');
		UARTCharPut(UARTA1_BASE, '\n');
		return 0;
	}else{
		UARTprintf("Top board is in DFU mode\r\n");
		return -1;
	}
}
