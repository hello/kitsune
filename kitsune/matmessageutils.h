
#ifndef _MATMESSAGEUTILS_H_
#define _MATMESSAGEUTILS_H_

#include "nanopb/pb.h"

#ifdef __cplusplus
extern "C" {
#endif

size_t SetInt16Matrix(pb_ostream_t * stream,const int16_t * data, int32_t rows, int32_t cols, uint32_t id);

    
    
#ifdef __cplusplus
}
#endif
    
#endif
