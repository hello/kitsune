/**
 * Top board control daemon
 */
#ifndef TOP_BOARD_H
#define TOP_BOARD_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

int top_board_task(void); //infinite loop
int Cmd_send_top(int argc, char *argv[]);
int top_board_dfu_begin(void);
#ifdef __cplusplus
}
#endif
    
#endif
