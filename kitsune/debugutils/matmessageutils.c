#include "matmessageutils.h"

#include "../nanopb/pb_encode.h"
#include "../protobuf/matrix.pb.h"



static void encode_int_array(pb_ostream_t *stream,IntArray_t * pdesc) {
    uint32_t i;
    
    for(i=0; i < pdesc->len; i++){
        switch (pdesc->type) {
                
            case esint8:
            {
                pb_encode_svarint(stream, pdesc->data.sint8[i]);
                break;
            }
            case euint8:
            {
                pb_encode_svarint(stream, pdesc->data.uint8[i]);
                break;
            }
            case esint16:
            {
                pb_encode_svarint(stream, pdesc->data.sint16[i]);
                break;
            }
            case euint16:
            {
                pb_encode_svarint(stream, pdesc->data.uint16[i]);
                break;
            }
            case esint32:
            {
                pb_encode_svarint(stream, pdesc->data.sint32[i]);
                break;
            }
            case euint32:
            {
                pb_encode_svarint(stream, pdesc->data.uint32[i]);
                break;
            }
                
            default:
                //log an error eventually
                break;
        }
    }
}

static bool write_int_mat(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    
    IntArray_t * pdesc = (IntArray_t *) (*arg);
    pb_ostream_t sizestream = {0};

    //write tag
    if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)) {
        return 0;
    }
    
    //find total size of this bad boy
    encode_int_array(&sizestream,pdesc);
    
    //write size
    if (!pb_encode_varint(stream, sizestream.bytes_written))
        return 0;
    
    //write payload
    encode_int_array(stream,pdesc);
    
    
    return 1;
    
}

size_t SetIntMatrix(pb_ostream_t * stream,IntArray_t data, int32_t rows, int32_t cols, uint32_t id,int64_t time) {
    
    Matrix mat;
    size_t size;
    
    memset(&mat,0,sizeof(Matrix));
    data.len = rows*cols;
    
    mat.id = id;
    mat.datatype = Matrix_DataType_INT;
    mat.idata.funcs.encode = write_int_mat;
    mat.idata.arg = &data;
    mat.time = time;
    mat.rows = rows;
    mat.cols = cols;

    
    pb_get_encoded_size(&size,Matrix_fields,&mat);
    pb_encode(stream,Matrix_fields,&mat);
    
    return size;
}

