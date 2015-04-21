#ifndef _DUMP_XSUM_H
#define _DUMP_XSUM_H

//Checksumming memory blocks for serialization
#define DUMP_XSUM_TYPE unsigned int
#define DUMP_XSUM_SIZE sizeof(DUMP_XSUM_TYPE)

#ifdef __cplusplus
extern "C" {
#endif

DUMP_XSUM_TYPE dump_xsum_compute( unsigned char * data, unsigned int size );
void dump_xsum_apply( unsigned char * data, unsigned int size );
void dump_xsum_verify( unsigned char * data, unsigned int size );

#ifdef __cplusplus
}
#endif

#endif
