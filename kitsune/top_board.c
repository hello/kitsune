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
#include "fs.h"
#include "uartstdio.h"

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
	hci_decode_handler_t hci_handler;
	struct{
		uint16_t crc;
		uint32_t len;
		uint32_t offset;
		long handle;

	}dfu_contex;
}self;
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
_next_file_data_block(uint8_t * write_buf, uint32_t buffer_size, uint32_t * out_actual_size){
	int status = sl_FsRead(self.dfu_contex.handle, self.dfu_contex.offset, write_buf, buffer_size);
	if(status > 0){
		self.dfu_contex.offset += status;
		*out_actual_size = status;
		return DFU_DATA_PACKET;
	}else{
		*out_actual_size = 0;
		return DFU_STOP_DATA_PACKET;
	}
}
static void
_on_message(uint8_t * message_body, uint32_t body_length){

}
static void
_close_and_reset_dfu(){
	sl_FsClose(self.dfu_contex.handle, 0,0,0);
	self.mode = TOP_NORMAL_MODE;
}
static void
_on_ack_failed(void){
	_close_and_reset_dfu();
}
static void
_on_decode_failed(void){
	if(self.mode == TOP_DFU_MODE){
		_close_and_reset_dfu();
	}
}
static void
_on_ack_success(void){
	vTaskDelay(50);
	if (self.mode == TOP_DFU_MODE) {
		switch (self.dfu_state) {
		case DFU_INVALID_PACKET:
			_close_and_reset_dfu();
			break;
		case DFU_START_DATA_PACKET: {
			self.dfu_state = DFU_INIT_PACKET;
			uint32_t init_packet[] = { (uint32_t) DFU_INIT_PACKET,
					(uint32_t) self.dfu_contex.crc };
			_encode_and_send((uint8_t*)init_packet, sizeof(init_packet));

		}
			break;
		case DFU_INIT_PACKET:
		case DFU_DATA_PACKET: {
			uint32_t block[32];
			uint32_t written;
			block[0] = DFU_DATA_PACKET;
			self.dfu_state = _next_file_data_block(
					((uint8_t*) block) + sizeof(uint32_t),
					(sizeof(block) - sizeof(uint32_t)), &written);
			_encode_and_send((uint8_t*)block, written + sizeof(block[0]));
		}
			break;
		case DFU_STOP_DATA_PACKET: {
			uint32_t end_packet[] = { DFU_STOP_DATA_PACKET };
			_encode_and_send((uint8_t*)end_packet, sizeof(end_packet));
			self.dfu_state = DFU_INVALID_PACKET;
		}
			break;
		}
	}
}
static void
_on_slip_message(uint8_t * c, uint32_t size){
	uint32_t err = hci_decode(c, size, &self.hci_handler);
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
	self.hci_handler = (hci_decode_handler_t){
			.on_message = _on_message,
			.on_ack_failed = _on_ack_failed,
			.on_ack_success = _on_ack_success,
			.on_decode_failed = _on_decode_failed
	};
	self.mode = TOP_NORMAL_MODE;
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
int _prep_file(char * name, uint32_t * out_fsize, uint16_t * out_crc, long * out_handle){
	if(!out_fsize || !out_crc){
		return -1;
	}
	uint8_t buffer[128];
	unsigned long tok = 0;
	long hndl, total = 0;
	int status = 0;
	uint16_t crc = 0xFFFFu;
	SlFsFileInfo_t info;
	sl_FsGetInfo(name, tok, &info);
	if(sl_FsOpen((unsigned char*)name, FS_MODE_OPEN_READ, &tok, &hndl)){
		UARTprintf("error opening for read %s.\r\n", name);
		return -1;
	}
	do{
		status = sl_FsRead(hndl, total, buffer, sizeof(buffer));
		if(status > 0){
			crc = hci_crc16_compute_cont(buffer,status,&crc);
			total += status;
		}

	}while(status > 0);

	//sl_FsClose(hndl, 0,0,0);
	*out_crc = crc;
	*out_fsize = total;
	*out_handle = hndl;
	UARTprintf("Bytes Read %u, crc = %u.\r\n", *out_fsize, *out_crc);
	return 0;
}

int top_board_dfu_begin(const char * bin){
	int ret;
	if(self.mode == TOP_NORMAL_MODE){
		self.mode = TOP_DFU_MODE;
		uint16_t crc;
		uint32_t len;
		long handle;
		ret = _prep_file("/top/factory.bin",&len, &crc, &handle);
		if(0 == ret){
			self.dfu_contex.crc = crc;
			self.dfu_contex.len = len;
			self.dfu_contex.offset = 0;
			self.dfu_contex.handle = handle;
			uint32_t begin_packet[] = { DFU_START_DATA_PACKET, len };
			_encode_and_send((uint8_t*) begin_packet, sizeof(begin_packet));
			self.dfu_state = DFU_START_DATA_PACKET;
		}else{
			self.mode = TOP_NORMAL_MODE;
			return ret;
		}
	}else{
		_close_and_reset_dfu();
		UARTprintf("Already in dfu mode\r\n");
	}
	return 0;

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
