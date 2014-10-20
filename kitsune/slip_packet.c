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
	uint8_t sequence_number;
	slip_handler_t handler;
	uint8_t * rx_buffer;
	uint32_t rx_idx;
	void (*handle_rx_byte)(uint8_t byte);
} self;

#define SLIP_END 0xC0
#define SLIP_ESC 0xDB
#define SLIP_ESC_END 0xDC
#define SLIP_ESC_ESC 0xDD

#define SLIP_RELIABLE_PACKET (0x1u << 7)
#define SLIP_INTEGRITY_CHECK (0x1u << 6)
#define SLIP_VENDOR_NORDIC_OPCODE 14u
#define SLIP_FRAME_SIZE 1u
#define SLIP_HEADER_SIZE 4u
#define SLIP_CRC_SIZE 2u

static uint8_t header_checksum_calculate(const uint8_t * hdr) {
	uint32_t checksum = hdr[0];
	checksum += hdr[1];
	checksum += hdr[2];
	checksum &= 0xFFu;
	checksum = (~checksum + 1u);
	return (uint8_t) checksum;
}
;

static uint16_t crc16_compute(const uint8_t * data, uint32_t size) {
	uint32_t i;
	uint16_t crc = 0xFFFF;
	for (i = 0; i < size; i++) {
		crc = (uint8_t) (crc >> 8) | (crc << 8);
		crc ^= data[i];
		crc ^= (uint8_t) (crc & 0xff) >> 4;
		crc ^= (crc << 8) << 4;
		crc ^= ((crc & 0xff) << 4) << 1;
	}
	return crc;
}
static bool
_slip_decode(uint8_t * buffer, uint32_t length){
	uint8_t * header = buffer;

	return true;
}
static void
_handle_slip_end(void){
	if(self.rx_buffer){
		if(self.handler.slip_on_message && _slip_decode(self.rx_buffer, self.rx_idx)){
			self.handler.slip_on_message(self.rx_buffer + SLIP_HEADER_SIZE, self.rx_idx - SLIP_HEADER_SIZE);
		}
		vPortFree(self.rx_buffer);
		self.rx_buffer = NULL;
		self.rx_idx = 0;
	}

}
static void _inc_seq(void) {
	//increase sequence number
	self.sequence_number = (self.sequence_number + 1) & 0x7;

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
void * slip_set_buffer(const uint8_t * orig, uint32_t raw_size,
		uint32_t * out_new_size) {
	uint32_t buffer_size = _calculate_raw_buffer(orig, raw_size);
	uint32_t total_size = SLIP_FRAME_SIZE + SLIP_HEADER_SIZE + buffer_size
			+ SLIP_CRC_SIZE + SLIP_FRAME_SIZE;
	void * ret = pvPortMalloc(total_size);
	if (ret) {
		uint8_t * frame_start = (uint8_t*) ret;
		uint8_t * header = (uint8_t *) (frame_start + SLIP_FRAME_SIZE);
		uint8_t * body = (uint8_t *) (header + SLIP_HEADER_SIZE);
		uint8_t * frame_end = (uint8_t*) (body + buffer_size + SLIP_CRC_SIZE);
		uint16_t * checksum = (uint16_t*) (body + buffer_size);

		_copy_slip_buffer_tx(body, orig, raw_size);
		_inc_seq();

		//package frame
		*frame_start = 0xC0;
		header[0] = (SLIP_RELIABLE_PACKET | SLIP_INTEGRITY_CHECK)
				+ self.sequence_number;
		//payload includes crc in nordic
		*(uint16_t*) (header + 1) = (uint16_t) (((buffer_size + SLIP_CRC_SIZE)
				<< 4) + SLIP_VENDOR_NORDIC_OPCODE);
		header[3] = header_checksum_calculate(header);
		memcpy(body, orig, buffer_size);
		*checksum = crc16_compute(header, buffer_size + SLIP_HEADER_SIZE);
		*frame_end = 0xC0;
		*out_new_size = total_size;

	}
	return ret;

}

//automatically incrememnt sequence number
//if checksum is enabled, an extra 2 bytes is appended at the end of the buffer
uint32_t slip_write(const uint8_t * orig, uint32_t buffer_size) {
	uint32_t new_size;
	uint8_t * tx_buffer = (uint8_t*) slip_set_buffer(orig, buffer_size,
			&new_size);
	if (tx_buffer) {
		//test
		if (self.handler.slip_display_char) {
			int i;
			for (i = 0; i < new_size; i++) {
				UARTprintf("0x%02X ", *(uint8_t*) (tx_buffer + i));
			}
		}
		//test end
		vPortFree(tx_buffer);
		return 0;
	} else {
		return 1;
	}

}

void slip_set_handler(const slip_handler_t * user) {
	self.handler = *user;
}

void slip_handle_rx(uint8_t c) {
	self.handle_rx_byte(c);
}
void slip_reset(void) {
	self.sequence_number = 0;
	self.handle_rx_byte = _handle_rx_byte_wait_start;
	self.rx_idx = 0;
	if(self.rx_buffer){
		vPortFree(self.rx_buffer);
		self.rx_buffer = NULL;
	}
}
