/**
 * Top board control daemon
 */
#ifndef TOP_BOARD_H
#define TOP_BOARD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void top_board_task(void * params);//infinite loop
int Cmd_send_top(int argc, char *argv[]);
int Cmd_top_dtm(int argc, char * argv[]);
int top_board_dfu_begin(const char * binname);

//call this when topboard boots
void top_board_notify_boot_complete(void);
//call this  when updating shasum of top update
//
void set_top_update_sha(const char * shasum, unsigned int imgnum);
//verify if update is valid
int verify_top_update(void);

void start_top_boot_watcher(void);
bool is_top_in_dfu(void);
int activate_top_ota(void);

#ifdef __cplusplus
}
#endif
    
#endif
