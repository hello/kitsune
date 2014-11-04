/**
 * Top board control daemon
 */
#ifndef UART_LOGGER_H
#define UART_LOGGER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#define UART_LOGGER_BLOCK_SIZE 512
typedef void (*on_upload_finished)(uint8_t result);
/**
 * logs a character to be uploaded to server
 */
void uart_logc(uint8_t c);
void uart_logger_task(void * params);
#ifdef __cplusplus
}
#endif
    
#endif
