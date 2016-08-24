#ifndef FILEDOWNLOADMANAGER_H_
#define FILEDOWNLOADMANAGER_H_

#include <stdint.h>
#include <stdbool.h>



void downloadmanagertask_init(uint16_t stack_size);
void update_file_download_status(bool is_pending);
bool add_to_file_error_queue(char* filename, int32_t err_code, bool write_error);



#endif /* FILEDOWNLOADMANAGER_H_ */
