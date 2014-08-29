#include "matmessageutils.h"

#include "../nanopb/pb_encode.h"
#include "../protobuf/matrix.pb.h"

typedef struct {
    const int16_t * data;
    uint32_t len;
} Int16Array_t;

static bool write_int16_mat(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    
    uint32_t i;
    Int16Array_t * pdesc = (Int16Array_t *) (*arg);
    const int16_t * data = pdesc->data;
    pb_ostream_t sizestream = {0};

    //write tag
    if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)) {
        return 0;
    }
    
    //find total size of this bad boy
    for(i=0; i < pdesc->len; i++){
        pb_encode_svarint(&sizestream, data[i]);
    }
    
    //write size
    if (!pb_encode_varint(stream, sizestream.bytes_written))
        return 0;
    
    //write payload
    for(i=0; i < pdesc->len; i++){
        pb_encode_svarint(stream, data[i]);
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
    mat.time = time;
    mat.rows = rows;
    mat.cols = cols;

    
    pb_get_encoded_size(&size,Matrix_fields,&mat);
    pb_encode(stream,Matrix_fields,&mat);
    
    return size;
}

