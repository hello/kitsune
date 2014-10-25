/*
 * slip_packet.c
 *
 *  Created on: Oct 16, 2014
 *      Author: jacksonchen
 */
#include "slip_packet.h"
#include "FreeRTOS.h"
#include "string.h"
#include <stdbool.h>

static void _handle_rx_byte_wait_start(uint8_t byte);
static void _handle_rx_byte_esc(uint8_t byte);
static void _handle_rx_byte_default(uint8_t byte) ;
static struct {
	slip_handler_t handler;
	uint8_t * rx_buffer;
	uint32_t rx_idx;
	void (*handle_rx_byte)(uint8_t byte);
	uint16_t dtm_msb;
	uint8_t dtm_has_msb;
} self;

#define SLIP_END 0xC0
#define SLIP_ESC 0xDB
#define SLIP_ESC_END 0xDC
#define SLIP_ESC_ESC 0xDD
#define SLIP_FRAME_SIZE 1u


#define SLIP_RELIABLE_PACKET (0x1u << 7)
#define SLIP_INTEGRITY_CHECK (0x1u << 6)
#define SLIP_VENDOR_NORDIC_OPCODE 14u

#define SLIP_HEADER_SIZE 4u
#define SLIP_CRC_SIZE 2u


static void
_handle_slip_end(void){

	if(self.rx_buffer){
		if(self.handler.slip_on_message){
			self.handler.slip_on_message(self.rx_buffer, self.rx_idx);
		}
		slip_reset(&self.handler);
	}

}

static uint32_t _calculate_raw_buffer(const uint8_t * orig, uint32_t size) {
	uint32_t i, ret = size;
	for (i = 0; i < size; i++) {
		if (orig[i] == SLIP_END || orig[i] == SLIP_ESC) {
			ret++;
		}
	}
	return ret;
}
static void _copy_slip_buffer_tx(uint8_t * dst, const uint8_t* orig,
		uint32_t orig_size) {
	//assumes dst has enough space
	uint32_t i;
	uint8_t * workptr = dst;
	for (i = 0; i < orig_size; i++) {
		if (orig[i] == SLIP_END) {
			*workptr++ = SLIP_ESC;
			*workptr++ = SLIP_ESC_END;
		} else if (orig[i] == SLIP_ESC) {
			*workptr++ = SLIP_ESC;
			*workptr++ = SLIP_ESC_ESC;

		} else {
			*workptr++ = orig[i];
		}
	}
}
static void _handle_rx_byte_default(uint8_t byte) {
	switch (byte) {
	case SLIP_END:
		_handle_slip_end();
		break;
	case SLIP_ESC:
		self.handle_rx_byte = _handle_rx_byte_esc;
		break;
	default:
		if(self.rx_buffer){
			self.rx_buffer[self.rx_idx++] = byte;
		}
		break;
	}
}
static void _handle_rx_byte_dtm(uint8_t byte){
	if(!self.dtm_has_msb){
		self.dtm_has_msb = 1;
		self.dtm_msb = byte;
	}else{
		self.dtm_has_msb = 0;
		self.handler.slip_on_dtm_event(self.dtm_msb << 8 + (uint16_t)byte);
	}
}
static void _handle_rx_byte_wait_start(uint8_t byte) {
	if (byte == SLIP_END) {
		if(self.rx_buffer){
			//this should not happen!
			vPortFree(self.rx_buffer);
		}
		self.rx_buffer = pvPortMalloc(SLIP_MAX_RX_BUFFER_SIZE);
		self.rx_idx = 0;
		self.handle_rx_byte = _handle_rx_byte_default;
	} else if (self.handler.slip_display_char) {
		self.handler.slip_display_char(byte);
	}
}
static void _handle_rx_byte_esc(uint8_t byte) {
	switch(byte){
	case SLIP_END:
		_handle_slip_end();
		break;
	case SLIP_ESC_END:
		if (self.rx_buffer) {
			self.rx_buffer[self.rx_idx++] = SLIP_END;
		}
		break;
	case SLIP_ESC_ESC:
		if (self.rx_buffer) {
			self.rx_buffer[self.rx_idx++] = SLIP_ESC;
		}
		break;
	default:
		if (self.rx_buffer) {
			self.rx_buffer[self.rx_idx++] = byte;
		}
		break;
	}
	self.handle_rx_byte = _handle_rx_byte_default;
}
static void *
_slip_set_buffer(const uint8_t * orig, uint32_t raw_size, uint32_t * out_new_size) {
	uint32_t new_size = _calculate_raw_buffer(orig, raw_size) + 2 * SLIP_FRAME_SIZE;
	uint8_t * ret = (uint8_t*)pvPortMalloc(new_size);
	if (ret) {
		_copy_slip_buffer_tx(ret + SLIP_FRAME_SIZE, orig, raw_size);
		ret[new_size - 1] = SLIP_END;
		ret[0] = SLIP_END;
		if(out_new_size){
			*out_new_size = new_size;
		}
	}
	return (void*)ret;

}

uint32_t slip_write(const uint8_t * orig, uint32_t buffer_size) {
	if (orig && buffer_size) {
		if(self.handle_rx_byte == _handle_rx_byte_dtm){
			//we are in dtm mode
			if(buffer_size == 2){
				self.handler.slip_put_char(orig[1]);
				self.handler.slip_put_char(orig[0]);
			}
		}else{
			uint32_t new_size;
			uint8_t * tx_buffer = (uint8_t*) _slip_set_buffer(orig, buffer_size,
					&new_size);
			if (tx_buffer) {
				//test
				if (self.handler.slip_display_char) {
					int i;
					for (i = 0; i < new_size; i++) {
						//UARTprintf("%02X ", *(uint8_t*) (tx_buffer + i));
						if (self.handler.slip_put_char) {
							//TODO take out the pointer checks by preset to default handlers
							self.handler.slip_put_char(tx_buffer[i]);
						}
					}
				}

				//test end
				vPortFree(tx_buffer);
				return 0;
			}
		}
	}
	return 1;


}

void slip_handle_rx(uint8_t c) {

	self.handle_rx_byte(c);
}
void   slip_reset(const slip_handler_t * user){
	self.handle_rx_byte = _handle_rx_byte_wait_start;
	self.rx_idx = 0;
	if(user){
		self.handler = *user;
	}
	if(self.rx_buffer){
		vPortFree(self.rx_buffer);
		self.rx_buffer = NULL;
	}
}

void slip_dtm_mode(void){
	slip_reset(&self.handler);
	self.handle_rx_byte = _handle_rx_byte_dtm;
}
