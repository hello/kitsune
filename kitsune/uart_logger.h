/**
 * Top board control daemon
 */
#ifndef UART_LOGGER_H
#define UART_LOGGER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//User Options
//keep this large to reduce HTTP request printing overheads
#define UART_LOGGER_BLOCK_SIZE 1280

//for reserved txbuf in the task for protobuf object
#define UART_LOGGER_RESERVED_SIZE 128

//needs to be sufficiently large to hold buffer for tx 2432
#define UART_LOGGER_THREAD_STACK_SIZE	(UART_LOGGER_BLOCK_SIZE + UART_LOGGER_RESERVED_SIZE + 2048)

//if you want to prepend a tag when calling LOGX() functions with TAGGED MODE
#define UART_LOGGER_PREPEND_TAG 0

//operation modes
//RAW: logs everything sent to PRINTF
//TAGGED: logs everything tagged with LOGX() functions
#define UART_LOGGER_MODE	UART_LOGGER_MODE_TAGGED

//how many files maximum to keep
#define UART_LOGGER_FILE_LIMIT 512

/**
 * Tag level defines
 */
#define LOG_INFO 	0x01
#define LOG_WARNING 0x02
#define LOG_ERROR	0x04
#define LOG_TIME	0x08
#define LOG_RADIO	0x10
#define LOG_VIEW_ONLY 0x20
#define LOG_FACTORY 0x40
#define LOG_TOP     0x80
/**
 * Mode defines
 */
#define UART_LOGGER_MODE_RAW 0
#define UART_LOGGER_MODE_TAGGED 1



/**
 * Utility macros, use these over logf
 */
#if UART_LOGGER_MODE == UART_LOGGER_MODE_TAGGED
#define LOGI(...) uart_logf(LOG_INFO, __VA_ARGS__)
#define LOGW(...) uart_logf(LOG_WARNING, __VA_ARGS__)
#define LOGE(...) uart_logf(LOG_ERROR, __VA_ARGS__)
#define LOGF(...) uart_logf(LOG_FACTORY, __VA_ARGS__)
#define LOGT(...) uart_logf(LOG_TOP, __VA_ARGS__)
#define DISP(...) uart_logf(LOG_VIEW_ONLY, __VA_ARGS__)
#else
#define LOGI(...) UARTprintf(__VA_ARGS__)
#define LOGW(...) UARTprintf(__VA_ARGS__)
#define LOGE(...) UARTprintf(__VA_ARGS__)
#define LOGF(...) UARTprintf(__VA_ARGS__)
#define DISP(...) UARTprintf(__VA_ARGS__)
#endif

/**
 * call this before any printf operation
 */
void uart_logger_init(void);
/**
 * For printing tags other than LOGX()
 */
void uart_logf(uint8_t tag, const char *pcString, ...);

/**
 * Emergency flush
 * should only be called by mcu reset, or before a reset function.
 * flushes the working buffer immediately instead of passing it to the worker task.
 * uart logger module will stop functioning right after.
 */
void uart_logger_flush(void);
void uart_logger_task(void * params);
int Cmd_log_upload(int argc, char *argv[]);
int Cmd_log_setview(int argc, char * argv[]);
void set_loglevel(uint8_t loglevel);
void uart_logc(uint8_t c);	//advanced: directly dumps character to tx block
#ifdef __cplusplus
}
#endif
    
#endif
