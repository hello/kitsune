#ifndef _PNSTREAM_H_
#define _PNSTREAM_H_

#include "hlo_stream.h"

void pn_stream_init(void);

hlo_stream_t * pn_read_stream_open(void);
hlo_stream_t * pn_write_stream_open(void);

int cmd_audio_self_test(int argc, char* argv[]);

#endif //_PNSTREAM_H_
