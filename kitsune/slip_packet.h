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

typedef struct{
	//default handler for unknown characters
	void (*slip_putchar)(uint8_t c);
	//user handles slip message
	void (*slip_on_message)(uint8_t * message, uint32_t size);
}slip_handler_t;

void slip_set_handler(const slip_handler_t * user);
void slip_handle_rx(uint8_t c);

//resets SEQ number in slip header
void   slip_reset(void);
//copies user buffer to a SLIP compatible buffer
//SEQ number is automatically generated
//returns dest
void * slip_write(const uint8_t * orig, uint32_t buffer_size);
//frees buffer
void slip_free_buffer(void * buffer);




#endif
