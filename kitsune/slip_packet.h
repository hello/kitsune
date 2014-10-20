/*
 * slip_packet.c
 *
 *  Created on: Oct 16, 2014
 *      Author: jacksonchen
 *  Prepares SLIP formated UART packet for DFU
 *
 */
#ifndef __MCASP_IF_H__
#define __MCASP_IF_H__

#include "stdint.h"

#define SLIP_MAX_TX_BUFFER_SIZE 256
#define SLIP_MAX_RX_BUFFER_SIZE 256
typedef struct{
	//default handler for unknown characters
	void (*slip_display_char)(uint8_t c);
	//user handles slip message
	void (*slip_on_message)(uint8_t * message, uint32_t size);
	void (*slip_put_char)(uint8_t c);
}slip_handler_t;

void slip_set_handler(const slip_handler_t * user);

//entry point to pass input here
void slip_handle_rx(uint8_t c);

//resets SEQ number in slip header
void   slip_reset(void);

//does not reset seq
uint32_t slip_write(const uint8_t * orig, uint32_t buffer_size);




#endif
