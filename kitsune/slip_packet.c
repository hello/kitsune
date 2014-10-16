/*
 * slip_packet.c
 *
 *  Created on: Oct 16, 2014
 *      Author: jacksonchen
 */
#include "slip_packet.h"
#include "FreeRTOS.h"

static struct{
	uint8_t sequence_number;
}self;

#define SLIP_RELIABLE_PACKET (0x1u << 7)
#define SLIP_INTEGRITY_CHECK (0x1u << 6)
#define SLIP_VENDOR_NORDIC_OPCODE 0x14u
#define SLIP_FRAME_SIZE 1u
#define SLIP_HEADER_SIZE 4u
#define SLIP_CRC_SIZE 2u

static uint8_t header_checksum_calculate(const uint8_t * hdr){
	uint32_t checksum = hdr[0];
	checksum += hdr[1];
	checksum += hdr[2];
	checksum &= 0xFFu;
	checksum = (~checksum + 1u);
	return (uint8_t)checksum;
}

static uint16_t crc16_compute(const uint8_t * data, uint32_t size){
	uint32_t i;
	uint16_t crc = 0xFFFF;
	for(i = 0; i < size; i++){
		crc = (uint8_t)(crc>>8) | (crc<<8);
		crc ^= data[i];
		crc ^= (uint8_t)(crc & 0xff) >> 4;
		crc ^= (crc <<8)<<4;
		crc ^= ((crc & 0xff)<<4)<<1;
	}
	return crc;
}
static void inc_seq(void){
	//increase sequence number
	self.sequence_number = (self.sequence_number + 1) & 0x7;

}
void   slip_reset(void){

}
//automatically incrememnt sequence number
//if checksum is enabled, an extra 2 bytes is appended at the end of the buffer
void * slip_write(const uint8_t * orig, uint32_t buffer_size){
	void * ret = pvPortMalloc(SLIP_FRAME_SIZE + SLIP_HEADER_SIZE + buffer_size + SLIP_CRC_SIZE + SLIP_FRAME_SIZE);
	if(ret){
		uint8_t * frame_start = (uint8_t*)ret;
		uint8_t * header = (uint8_t *)(frame_start+SLIP_FRAME_SIZE);
		uint8_t * body = (uint8_t *)(header + SLIP_HEADER_SIZE);
		uint8_t * frame_end = (uint8_t*)(body + buffer_size + SLIP_CRC_SIZE);
		uint16_t * checksum = (uint16_t*)(body + buffer_size);

		inc_seq();

		//package frame
		*frame_start = 0xC0;
		header[0] = (SLIP_RELIABLE_PACKET | SLIP_INTEGRITY_CHECK) + self.sequence_number;
		*(uint16_t*)(header+1) = ((uint16_t)buffer_size <<4) + SLIP_VENDOR_NORDIC_OPCODE;
		header[3] = header_checksum_calcualte(header);
		memcpy(body, orig, buffer_size);
		*checksum = crc16_compute(orig,buffer_size);
		*frame_end = 0xC0;

	}
	return ret;

}
//frees buffer
void * slip_free(void * buffer){
	return vPortFree(buffer);
}
