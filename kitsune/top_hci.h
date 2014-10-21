/**
 * Top board control daemon
 */
#ifndef TOP_BOARD_H
#define TOP_BOARD_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef struct{
	void (*on_message)(uint8_t * message_body, uint32_t body_length);
	void (*on_failed)(void);
}hci_decode_handler_t;

void hci_init(void);
uint32_t hci_decode(uint8_t * raw, uint32_t length, const hci_decode_handler_t * handler);
uint8_t * hci_encode(uint8_t * body, uint32_t body_length, uint32_t * out_encoded);
uint16_t hci_crc16_compute(uint8_t * raw, uint32_t length);
void hci_free(uint8_t * encoded_message);

#ifdef __cplusplus
}
#endif
    
#endif
