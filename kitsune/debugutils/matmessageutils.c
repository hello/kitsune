#include "matmessageutils.h"

#include "pb_encode.h"
#include "pb_decode.h"
#include "matrix.pb.h"



bool write_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
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

bool write_bytes(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
	bytes_desc_t * desc = (bytes_desc_t *)(*arg);


    //write tag
    if (!pb_encode_tag(stream, PB_WT_STRING, field->tag)) {
        return 0;
    }


    pb_encode_string(stream,desc->bytes, desc->len);

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

bool write_mat_array(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    MatrixListEncodeContext_t * context = (MatrixListEncodeContext_t  * )(*arg);
    const_MatDesc_t desc;
    pb_ostream_t sizestream;
    uint8_t isFirst = true;
    uint8_t state;
    
    
    
    do {
        state = context->func(isFirst,&desc,context->data);
        
        if (state == MAT_MESSAGE_FAIL) {
            break; //still return 1, just means buffers are empty
        }

        isFirst = false;
        
        if (!pb_encode_tag(stream,PB_WT_STRING, field->tag)) {
            return 0;
        }
        
        //reset size stream
        memset(&sizestream,0,sizeof(sizestream));

        //get size
        SetIntMatrix(&sizestream, desc.id, desc.tags, desc.source, desc.data, desc.rows, desc.cols, desc.t1, desc.t2);

        //write size
        if (!pb_encode_varint(stream, sizestream.bytes_written)) {
            return 0;
        }
        
        //encode matrix payload
        SetIntMatrix(stream, desc.id, desc.tags, desc.source, desc.data, desc.rows, desc.cols, desc.t1, desc.t2);
    
        
    } while (state == MAT_MESSAGE_CONTINUE);

    return 1;
    
}

static bool read_int_array(pb_istream_t *stream,IntArray_t * pdesc) {
    int64_t value;
    uint32_t i = 0;
    
    while(stream->bytes_left > 0) {
        
        if (!pb_decode_svarint(stream, &value)) {
            return false;
        }

        switch (pdesc->type) {
                
            case esint8:
            {
                pdesc->data.sint8[i] = value;
                break;
            }
            case euint8:
            {
                pdesc->data.uint8[i] = value;
                break;
            }
            case esint16:
            {
                pdesc->data.sint16[i] = value;
                break;
            }
            case euint16:
            {
                pdesc->data.uint16[i] = value;
                break;
            }
            case esint32:
            {
                pdesc->data.sint32[i] = value;
                break;
            }
            case euint32:
            {
                pdesc->data.uint32[i] = value;
                break;
            }
                
            default:
                //log an error eventually
                break;
        }
        
        i++;
    }
    
    return true;
}

bool read_string(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    StringDesc_t * p = (StringDesc_t *) (*arg);
    /* We could read block-by-block to avoid the large buffer... */

    if (p->maxlen < stream->bytes_left) {
        return false;
    }


    if (!pb_read(stream, p->writebuf, stream->bytes_left))
        return false;

    /* Print the string, in format comparable with protoc --decode.
     * Format comes from the arg defined in main().
     */
    //printf((char*)*arg, buffer);
    return true;
}


bool read_mat_array(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    IntArray_t * p = (IntArray_t *)(*arg);
    
    //max size comes in through the array parameters, and is in bytes
    if (p->len < stream->bytes_left) {
        return false;
    }
    
    if (!read_int_array(stream,p)) {
        return false;
    }
    
    return true;

}


size_t SetIntMatrix(pb_ostream_t * stream,
                    const char * id,
                    const char * tags,
                    const char * source,
                    const_IntArray_t data,
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

uint8_t GetIntMatrix(MatDesc_t * matdesc, pb_istream_t * stream,size_t string_maxsize) {

    Matrix mat;
    
    //id
    StringDesc_t desc1;
    desc1.maxlen = string_maxsize;
    desc1.writebuf = (uint8_t *)matdesc->id;
    
    mat.id.arg = &desc1;
    mat.id.funcs.decode = read_mat_array;
    
    //tags
    StringDesc_t desc2;
    desc2.maxlen = string_maxsize;
    desc2.writebuf = (uint8_t *)matdesc->tags;
    
    mat.tags.arg = &desc2;
    mat.tags.funcs.decode = read_mat_array;
    
    //source
    StringDesc_t desc3;
    desc3.maxlen = string_maxsize;
    desc3.writebuf = (uint8_t *)matdesc->source;
    
    mat.tags.arg = &desc3;
    mat.tags.funcs.decode = read_mat_array;
    
    
    //the sweet,sweet payload
    mat.idata.arg = &matdesc->data;
    mat.idata.funcs.decode = read_mat_array;
    
    if (pb_decode(stream, Matrix_fields, &mat)) {
        matdesc->t1 = mat.time1;
        matdesc->t2 = mat.time2;
        matdesc->rows = mat.rows;
        matdesc->cols = mat.cols;
        
        return true;
    }
    
    return false;
}


