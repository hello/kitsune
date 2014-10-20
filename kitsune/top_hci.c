#include "top_hci.h"
#include <string.h>

#define HCI_RELIABLE_PACKET (0x1u << 7)
#define HCI_INTEGRITY_CHECK (0x1u << 6)
#define HCI_VENDOR_NORDIC_OPCODE 14u
#define HCI_HEADER_SIZE 4u
#define HCI_CRC_SIZE 2u
static struct{
	uint8_t sequence_number;
}self;

static void _inc_seq(void) {
	//increase sequence number
	self.sequence_number = (self.sequence_number + 1) & 0x7;

}
static uint8_t
_header_checksum_calculate(const uint8_t * hdr) {
	uint32_t checksum = hdr[0];
	checksum += hdr[1];
	checksum += hdr[2];
	checksum &= 0xFFu;
	checksum = (~checksum + 1u);
	return (uint8_t) checksum;
}

static uint16_t
_crc16_compute(const uint8_t * data, uint32_t size) {
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

uint8_t * hci_decode(uint8_t * raw, uint32_t length, uint32_t * out_decoded_len){
	if(length <= HCI_HEADER_SIZE){
		UARTprintf("header size fail\r\n");
		return NULL;
	}
	if( !(raw[0] & HCI_RELIABLE_PACKET) ){
		UARTprintf("Flag Fail \r\n");
		return NULL;
	}
	if( !(raw[0] & HCI_INTEGRITY_CHECK) ){
		UARTprintf("Flag Fial 2 \r\n");
			return NULL;
	}
	if((raw[1] & 0x0Fu) != HCI_VENDOR_NORDIC_OPCODE){
		UARTprintf("VENDOR FAIL  \r\n");
		return NULL;
	}

	const uint32_t expected_checksum = (raw[0] + raw[1] + raw[2] + raw[3]) & 0xFFu;
	if(expected_checksum != 0){
		UARTprintf("Header Checksu fail \r\n");
		return NULL;
	}
	uint16_t crc_calculated = _crc16_compute(raw, (length - HCI_CRC_SIZE));
	//uint16_t crc_received = raw[length - HCI_CRC_SIZE + 1] << 8 + raw[length - HCI_CRC_SIZE];
	uint16_t crc_received = *(uint16_t*)(raw+length-HCI_CRC_SIZE);
	if(crc_received != crc_calculated){
		UARTprintf("Body Checksum fail cal%x rcvd %x\r\n",crc_calculated, crc_received);
		return NULL;
	}
	if(out_decoded_len){
		*out_decoded_len = length - HCI_HEADER_SIZE - HCI_CRC_SIZE;
	}
	return raw + HCI_HEADER_SIZE;

}

uint8_t * hci_encode(uint8_t * message_body, uint32_t body_length, uint32_t * out_encoded_len){
	uint32_t total_size = HCI_HEADER_SIZE + body_length + HCI_CRC_SIZE;
	void * ret = pvPortMalloc(total_size);
	if(ret){
		uint8_t * header = (uint8_t *) (ret);
		uint8_t * body = (uint8_t *) (header + HCI_HEADER_SIZE);
		uint16_t * checksum = (uint16_t*) (body + body_length);
		_inc_seq();
		//pack header
		header[0] = (HCI_RELIABLE_PACKET | HCI_INTEGRITY_CHECK)
						+ self.sequence_number;
		*(uint16_t*) (header + 1) = (uint16_t) (((body_length + HCI_CRC_SIZE)
						<< 4) + HCI_VENDOR_NORDIC_OPCODE);
		header[3] = _header_checksum_calculate(header);
		memcpy(body, message_body, body_length);
		*checksum = _crc16_compute(header, body_length + HCI_HEADER_SIZE);
		if(out_encoded_len){
			*out_encoded_len = total_size;
		}

	}
	return ret;
}
void hci_free(uint8_t * encoded_message){
	vPortFree(encoded_message);
}
