/*
 * slip_packet.c
 *
 *  Created on: Oct 16, 2014
 *      Author: jacksonchen
 *  Prepares SLIP formated UART packet for DFU
 *
 */
#ifndef SLIP_PACKET_H
#define SLIP_PACKET_H

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

//initialization functions, must be called before handle_rx
void   slip_reset(const slip_handler_t * user);

//entry point to pass input here
void slip_handle_rx(uint8_t c);

//writes hci packet in slip format to uart
uint32_t slip_write(const uint8_t * orig, uint32_t buffer_size);



#endif
