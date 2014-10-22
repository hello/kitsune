
#ifndef _MATMESSAGEUTILS_H_
#define _MATMESSAGEUTILS_H_

#include "pb.h"

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
    } const_IntArray_t;
    
    typedef struct {
        union {
            int8_t * sint8;
            uint8_t * uint8;
            
            int16_t * sint16;
            uint16_t * uint16;
            
            int32_t * sint32;
            uint32_t * uint32;
            
        } data;
        EIntType type;
        uint32_t len;
    } IntArray_t;
    
    typedef struct {
        const char * id;
        const char * tags;
        const char * source;
        const_IntArray_t data;
        int32_t rows;
        int32_t cols;
        int64_t t1;
        int64_t t2;
    } const_MatDesc_t;
    
    typedef struct {
        char * id;
        char * tags;
        char * source;
        IntArray_t data;
        int32_t rows;
        int32_t cols;
        int64_t t1;
        int64_t t2;
    } MatDesc_t;
    
    typedef uint8_t (*GetNextMatrixFunc_t)(uint8_t isFirst,const_MatDesc_t * pdesc);
    
    size_t SetIntMatrix(pb_ostream_t * stream,
                        const char * id,
                        const char * tags,
                        const char * source,
                        const_IntArray_t data,
                        int32_t rows,
                        int32_t cols,
                        int64_t t1,
                        int64_t t2);
    
    
    
    
    size_t SetMatrixMessage(pb_ostream_t * stream,
                            const char * macbytes,
                            uint32_t unix_time,
                            GetNextMatrixFunc_t get_next_mat_func);
    
    uint8_t GetIntMatrix(MatDesc_t * mymat, pb_istream_t * stream,size_t maxsize);
    
    
#ifdef __cplusplus
}
#endif

#endif
