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
#define UART_LOGGER_RETRY_TIMES_IN_TICKS 5000
#define UART_LOGGER_PREPEND_TAG 1


/**
 * Tag level defines
 */
#define LOG_INFO 	0x01
#define LOG_WARNING 0x02
#define LOG_ERROR	0x04
#define LOG_TIME	0x08
#define LOG_RADIO	0x10

#define LOGI(...) uart_logf(LOG_INFO, __VA_ARGS__)
#define LOGW(...) uart_logf(LOG_WARNING, __VA_ARGS__)
#define LOGE(...) uart_logf(LOG_ERROR, __VA_ARGS__)

void uart_logf(uint8_t tag, const char *pcString, ...);

/**
 * logs a character to be uploaded to server
 */
void uart_logc(uint8_t c);
void uart_logger_task(void * params);
int Cmd_log_upload(int argc, char *argv[]);
int Cmd_log_setview(int argc, char * argv[]);
#ifdef __cplusplus
}
#endif
    
#endif
