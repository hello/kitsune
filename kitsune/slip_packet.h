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

#ifdef __cplusplus
extern "C"
{
#endif



//resets SEQ number in slip header
void   slip_reset(void);
//copies user buffer to a SLIP compatible buffer
//SEQ number is automatically generated
//returns dest
void * slip_write(const uint8_t * orig, uint32_t buffer_size);
//frees buffer
void * slip_free_buffer(void * buffer);




#ifdef __cplusplus
}
#endif
#endif
