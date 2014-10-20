/**
 * Top board control daemon
 */
#ifndef TOP_BOARD_H
#define TOP_BOARD_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

uint8_t * hci_decode(uint8_t * raw, uint32_t raw_size, uint32_t * out_decoded_len);
uint8_t * hci_encode(uint8_t * body, uint32_t body_length, uint32_t * out_encoded_len);
void hci_free(uint8_t * encoded_message);

#ifdef __cplusplus
}
#endif
    
#endif
