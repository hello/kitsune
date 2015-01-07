#ifndef __PILL_SETTINGS__
#define __PILL_SETTINGS__

#include <stdlib.h>

#include "nanopb/pb_decode.h"
#include "nanopb/pb_encode.h"
#include "protobuf/sync_response.pb.h"

#define PILL_SETTING_FILE   "/hello/pill_settings"
#define MAX_PILL_SETTINGS_COUNT      2

#ifdef __cplusplus
extern "C" {
#endif

int pill_settings_save(const BatchedPillSettings* pill_settings);
int pill_settings_load_from_file();
uint32_t pill_settings_get_color(const char* pill_id);
int pill_settings_reset_all();
int pill_settings_init();

#ifdef __cplusplus
}
#endif

#endif
