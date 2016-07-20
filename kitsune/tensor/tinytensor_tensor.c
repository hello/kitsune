#include "tinytensor_tensor.h"
#include "tinytensor_memory.h"

#include <assert.h>

void delete_tensor(void * context) {
    Tensor_t * p = (Tensor_t *) context;
    
    FREE(p->x);
    FREE(p);
}

void tinytensor_zero_out_tensor(Tensor_t * tensor) {
    uint32_t i;
    uint32_t num_elements= tensor->dims[0];
    for (i = 1; i < TENSOR_DIM; i++) {
        num_elements *= tensor->dims[i];
    }
    
    memset(tensor->x,0,num_elements*sizeof(Weight_t));
}


Tensor_t * tinytensor_create_new_tensor(const uint32_t dims[TENSOR_DIM]) {
    uint32_t i;
    uint32_t num_elements= dims[0];
    for (i = 1; i < TENSOR_DIM; i++) {
        num_elements *= dims[i];
    }
    Tensor_t * tensor = (Tensor_t *)MALLLOC(sizeof(Tensor_t));
    MEMSET(tensor,0,sizeof(Tensor_t));
    MEMCPY(tensor->dims, dims, sizeof(tensor->dims));
    tensor->x = MALLLOC(num_elements * sizeof(Weight_t));
    tensor->delete_me = delete_tensor;
    
    return tensor;
}

Tensor_t * tinytensor_clone_new_tensor(const ConstTensor_t * in) {
    uint32_t num_elements= in->dims[0];
    uint32_t i;
    for (i = 1; i < TENSOR_DIM; i++) {
        num_elements *= in->dims[i];
    }
    
    Tensor_t * tensor = tinytensor_create_new_tensor(in->dims);
    memcpy(tensor->x,in->x,sizeof(Weight_t) * num_elements);
    tensor->scale = in->scale;
    return tensor;
}


Tensor_t * tinytensor_create_new_transposed_tensor(const Tensor_t * in) {
    uint32_t dims[4];
    uint32_t i,j;
    Tensor_t * out;
    Weight_t * po;
    Weight_t * pi;
   
    
    //get transposed dims
    dims[0] = in->dims[0];
    dims[1] = in->dims[1];
    dims[2] = in->dims[3];
    dims[3] = in->dims[2];
    
    assert(dims[0] == 1 && dims[1] == 1);
    
    out = tinytensor_create_new_tensor(dims);

    po = out->x;
    for (i = 0; i < in->dims[3]; i++) {
        pi = in->x + i;
        for (j = 0; j < in->dims[2]; j++) {
            *po = *pi;
            po++;
            pi += in->dims[3];
        }
    }
    
    return out;
}
