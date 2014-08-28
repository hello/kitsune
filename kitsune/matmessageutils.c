#include "matmessageutils.h"

#include "nanopb/pb_encode.h"
#include "protobuf/matrix.pb.h"

typedef struct {
    const int16_t * data;
    uint32_t len;
} Int16Array_t;

static bool write_int16_mat(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    
    uint32_t j;
    Int16Array_t * pdesc = (Int16Array_t *) (*arg);
    const int16_t * data = pdesc->data;

    pb_encode_tag(stream, PB_WT_STRING, field->tag);
        
    //write length
    pb_encode_svarint(stream, pdesc->len);
    
    for(j = 0; j < pdesc->len; j++ ) {
        pb_encode_svarint(stream, data[j]);
    }
    
    return 1;
    
}

size_t SetInt16Matrix(pb_ostream_t * stream,const int16_t * data, int32_t rows, int32_t cols, uint32_t id, int64_t time) {
    
    Matrix mat;
    Int16Array_t desc;
    size_t size;
    
    memset(&mat,0,sizeof(Matrix));
    desc.data = data;
    desc.len = rows*cols;
    
    mat.id = id;
    mat.datatype = Matrix_DataType_INT;
    mat.idata.funcs.encode = write_int16_mat;
    mat.idata.arg = &desc;
    mat.has_time = 1;
    mat.time = time;
    
    pb_get_encoded_size(&size,Matrix_fields,&mat);
    pb_encode(stream,Matrix_fields,&mat);
    
    return size;
}

