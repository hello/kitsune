
#ifndef _MATMESSAGEUTILS_H_
#define _MATMESSAGEUTILS_H_

#include "pb.h"
#include <stdbool.h>

#define MAT_MESSAGE_FAIL (0)
#define MAT_MESSAGE_CONTINUE (1)
#define MAT_MESSAGE_DONE (2)


#ifdef __cplusplus
extern "C" {
#endif
	typedef struct {
		uint8_t * writebuf;
		size_t maxlen;
	} StringDesc_t;

	typedef struct {
		uint8_t * bytes;
		uint32_t len;
	} bytes_desc_t;

	bool write_bytes(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);

	bool write_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
    
	bool read_string(pb_istream_t *stream, const pb_field_t *field, void **arg);

    bool write_mat_array(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);

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
    
    typedef uint8_t (*GetNextMatrixFunc_t)(uint8_t isFirst,const_MatDesc_t * pdesc, void * encodedata);
    
    typedef struct {
        void * data;
        GetNextMatrixFunc_t func;
    } MatrixListEncodeContext_t;
    
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
            				uint8_t * macbytes,
                            const char * device_id,
                            uint32_t unix_time,
                            MatrixListEncodeContext_t * matrix_list_context);
    
    uint8_t GetIntMatrix(MatDesc_t * mymat, pb_istream_t * stream,size_t maxsize);
    
#ifdef __cplusplus
}
#endif

#endif
