#include "top_board.h"
#include "slip_packet.h"
#include "rom_map.h"
#include <hw_types.h>
#include <uart.h>
#include <hw_memmap.h>
#include <prcm.h>
#include <stdint.h>
#include <string.h>

static struct{
	enum{
		TOP_NORMAL_MODE = 0,
		TOP_DFU_MODE
	}mode;
}self;

static void
_printchar(uint8_t c){
	UARTCharPutNonBlocking(UARTA0_BASE, c); //basic feedback
}

static void
_on_slip_message(uint8_t * c, uint32_t size){

	char * ret = hci_decode(c, size, NULL);
	if(ret){
		UARTprintf("Got a HCI message: %s\r\n", ret);
	}else{
		UARTprintf("HCI decode fail\r\n");
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
	MAP_UARTConfigSetExpClk(UARTA1_BASE, PRCMPeripheralClockGet(PRCM_UARTA1),
			38400,
			(UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
	while (1) {
		uint8_t c = UARTCharGet(UARTA1_BASE);
		slip_handle_rx(c);
	}
	return 1;
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
