#ifndef _TINYTENSOR_TENSOR_H_
#define _TINYTENSOR_TENSOR_H_

#include "tinytensor_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MEMORY LAYOUT OF THE "TENSOR"
 
    A SERIES OF 2-D MATRICES
 
    A 2-D MATRIX IS row1,row2,row3,.....  
    THE STRIDE IS THE NUMBER OF COLUMNS
 
    so for the 3D matrix, a.k.a the tensor
    the stride is NCOLS * NROWS
 
    mat1_row1, mat1_row2... mat2_row1, mat2_row2.... matN_rowM
 
 */

    
Tensor_t * tinytensor_create_new_tensor(const uint32_t dims[TENSOR_DIM]);

Tensor_t * tinytensor_clone_new_tensor(const ConstTensor_t * in);

void tinytensor_zero_out_tensor(Tensor_t * tensor);

Tensor_t * tinytensor_create_new_transposed_tensor(const Tensor_t * in);


#ifdef __cplusplus
}
#endif

#endif // _TINYTENSOR_TENSOR_H_
