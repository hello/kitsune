#ifndef HLO_LOSSLESS_H
#define HLO_LOSSLESS_H
#include "hlo_pipe.h"
int hlo_filter_lossless_encoder(hlo_stream_t * input, hlo_stream_t * output, void * ctx, hlo_stream_signal signal);
int hlo_lossless_start_stream( hlo_stream_t * output);
int hlo_lossless_write_frame(hlo_stream_t * output, int16_t short_samples[] );

hlo_stream_t * hlo_lossless_init_chunkbuf( int size );
int hlo_lossless_write_chunkbuf( hlo_stream_t * s, int16_t short_samples[] );
int hlo_lossless_dump_chunkbuf( hlo_stream_t * s, hlo_stream_t * output );
#endif
