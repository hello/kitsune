
#ifndef _MATMESSAGEUTILS_H_
#define _MATMESSAGEUTILS_H_

#include "../nanopb/pb.h"

#ifdef __cplusplus
extern "C" {
#endif
    
    typedef enum {
        euint8,
        esint8,
        esint16,
        euint16,
        euint32,
        esint32
    } EIntType;
    
    typedef struct {
        union {
            const int8_t * sint8;
            const uint8_t * uint8;
            
            const int16_t * sint16;
            const uint16_t * uint16;
            
            const int32_t * sint32;
            const uint32_t * uint32;
            
        } data;
        EIntType type;
        uint32_t len;
    } IntArray_t;
    
    size_t SetIntMatrix(pb_ostream_t * stream,
                        const char * id,
                        const char * tags,
                        const char * source,
                        IntArray_t data,
                        int32_t rows,
                        int32_t cols,
                        int64_t t1,
                        int64_t t2);
    
    
#ifdef __cplusplus
}
#endif

#endif
