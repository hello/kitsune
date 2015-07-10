#ifndef __PILL_SETTINGS__
#define __PILL_SETTINGS__

#include <stdlib.h>

#include "nanopb/pb_decode.h"
#include "nanopb/pb_encode.h"
#include "protobuf/sync_response.pb.h"

#define PILL_SETTING_FILE   "/hello/pills"
#define MAX_PILL_SETTINGS_COUNT      2

#ifdef __cplusplus
extern "C" {
#endif

bool on_pill_settings(pb_istream_t *stream, const pb_field_t *field, void **arg);
int pill_settings_load_from_file();
uint32_t pill_settings_get_color(const char* pill_id);
int pill_settings_reset_all();
int pill_settings_init();
int pill_settings_pill_count();

#ifdef __cplusplus
}
#endif

#endif
