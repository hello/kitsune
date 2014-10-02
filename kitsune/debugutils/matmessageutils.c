#include "matmessageutils.h"

#include "pb_encode.h"
#include "protobuf/matrix.pb.h"

typedef struct {
    const MatDesc_t * data;
    uint16_t len;
} MatDescArray_t;

static bool write_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    const char * str = (const char *)(*arg);
    static const char nullchar = '\0';
    
    if (!str) {
        str = &nullchar;
    }
    
    //write tag
    if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)) {
        return 0;
    }
    
    
    pb_encode_string(stream, (uint8_t*)str, strlen(str));
 
    
    
    return 1;

}

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
    if (!pb_encode_varint(stream, sizestream.bytes_written)) {
        return 0;
    }
    
    //write payload
    encode_int_array(stream,pdesc);
    
    
    return 1;
    
}

static bool write_mat_array(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    MatDescArray_t * pdesc = (MatDescArray_t *)(*arg);
    const MatDesc_t * p;
    uint16_t i;
    pb_ostream_t sizestream;

    for (i = 0; i < pdesc->len; i++) {
        p = &pdesc->data[i];
        
        if (!pb_encode_tag(stream,PB_WT_STRING, field->tag)) {
            return 0;
        }
        
        //reset size stream
        memset(&sizestream,0,sizeof(sizestream));

        //get size
        SetIntMatrix(&sizestream, p->id, p->tags, p->source, p->data, p->rows, p->cols, p->t1, p->t2);

        //write size
        if (!pb_encode_varint(stream, sizestream.bytes_written)) {
            return 0;
        }
        
        //encode matrix payload
        SetIntMatrix(stream, p->id, p->tags, p->source, p->data, p->rows, p->cols, p->t1, p->t2);
        
    }

    return 1;
    
}

size_t SetIntMatrix(pb_ostream_t * stream,
                    const char * id,
                    const char * tags,
                    const char * source,
                    IntArray_t data,
                    int32_t rows,
                    int32_t cols,
                    int64_t t1,
                    int64_t t2) {
    
    Matrix mat;
    size_t size = 0;
    
    memset(&mat,0,sizeof(Matrix));
    data.len = rows*cols;
    
    mat.id.arg = (void *) id;
    mat.id.funcs.encode = write_string;
    
    mat.tags.arg = (void *)tags;
    mat.tags.funcs.encode = write_string;
    
    mat.source.arg = (void *)source;
    mat.source.funcs.encode = write_string;
    
    mat.datatype = Matrix_DataType_INT;
    
    mat.idata.funcs.encode = write_int_mat;
    mat.idata.arg = &data;
    
    mat.time1 = t1;
    
    mat.time2 = t2;
    
    mat.rows = rows;
    
    mat.cols = cols;

    
    pb_get_encoded_size(&size,Matrix_fields,&mat);
    
    if (stream) {
        pb_encode(stream,Matrix_fields,&mat);
    }
    
    return size;
}


size_t SetMatrixMessage(pb_ostream_t * stream,
                        const char * macbytes,
                        uint32_t unix_time,
                        const MatDesc_t * mats,
                        uint16_t nummats) {
    
    size_t size = 0;

    MatrixClientMessage mess;
    MatDescArray_t desc;
    
    desc.data = mats;
    desc.len = nummats;
    
    mess.unix_time = unix_time;
    mess.has_unix_time = 1;
    
    mess.mac.funcs.encode = write_string;
    mess.mac.arg = (void*)macbytes;
    
    mess.has_matrix_payload = 0;
    
    mess.matrix_list.funcs.encode = write_mat_array;
    mess.matrix_list.arg = (void *)&desc;
    
    pb_get_encoded_size(&size,MatrixClientMessage_fields,&mess);
    
    if (stream) {
        pb_encode(stream,MatrixClientMessage_fields,&mess);
    }
    
    return size;
}


