#include "tinytensor_math.h"
#include "kit_assert.h"

//not including header because of macro definition conflicts
extern uint32_t FixedPointExp2Q10(const int16_t x);

__attribute__((section(".data")))
const static int8_t tanh_table[356] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,28,29,30,31,32,33,34,35,36,37,38,39,40,41,41,42,43,44,45,46,47,48,48,49,50,51,52,53,53,54,55,56,57,57,58,59,60,61,61,62,63,64,64,65,66,67,67,68,69,69,70,71,72,72,73,74,74,75,76,76,77,77,78,79,79,80,81,81,82,82,83,83,84,85,85,86,86,87,87,88,88,89,89,90,90,91,91,92,92,93,93,94,94,95,95,95,96,96,97,97,98,98,98,99,99,100,100,100,101,101,101,102,102,103,103,103,104,104,104,105,105,105,106,106,106,106,107,107,107,108,108,108,108,109,109,109,110,110,110,110,111,111,111,111,112,112,112,112,112,113,113,113,113,114,114,114,114,114,115,115,115,115,115,115,116,116,116,116,116,116,117,117,117,117,117,117,118,118,118,118,118,118,118,119,119,119,119,119,119,119,119,120,120,120,120,120,120,120,120,120,121,121,121,121,121,121,121,121,121,121,122,122,122,122,122,122,122,122,122,122,122,122,123,123,123,123,123,123,123,123,123,123,123,123,123,123,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,124,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,126,127};


#define tinymath_abs_int32(x) ((x) > 0 ? (x): (-x));

void tinytensor_descale(Weight_t * y, int8_t * out_scale, int32_t x, int8_t in_scale) {
    //make it fit in 8 bits
    uint8_t i;
    int32_t ux = tinymath_abs_int32(x);
    
    for (i = 0; i < 24; i++) {
        if (  (ux >> i) <= 127 ) {
            break;
        }
    }
    
    *out_scale = in_scale - i;
    *y = x >> i;
    
}

void tinytensor_tanh(Weight_t * y, int8_t * out_scale, int32_t x,int8_t in_scale) {
    const static uint32_t k_max_len = sizeof(tanh_table) / sizeof(tanh_table[0]);
    const uint8_t sign = x < 0;
    Weight_t yy = 0x7F;
    
    *out_scale = 0;
    
    x = tinymath_abs_int32(x);
    
    if (in_scale > 0) {
        x >>= in_scale;
    }
    
    if (in_scale < 0) {
        x <<= -in_scale;
    }
    
    if (x < k_max_len) {
        yy = tanh_table[x];
    }


    if (sign) {
        *y = -yy;
        return;
    }

    *y = yy;
}

//(tanh + 1) / 2 == sigmoid
//crud, but should work okay
void tinytensor_sigmoid(Weight_t * y, int8_t * out_scale, int32_t x,int8_t in_scale) {
    Weight_t tanh;
    int16_t temp16;
    
    tinytensor_tanh(&tanh,out_scale,x,in_scale);
    temp16 = tanh;
    
    //add one
    temp16 += (1 << QFIXEDPOINT);
    
    //divide by two
    temp16 >>= 1;
    
#if 0 //pointless test?
    if (temp16 > MAX_WEIGHT) {
        temp16 = MAX_WEIGHT;
    }
#endif
    
    *y = (Weight_t)temp16;
    *out_scale = 0;
}


void tinytensor_vec_softmax_in_place(Weight_t * xvec, uint32_t len, int8_t in_scale) {
    uint32_t i;
    int32_t temp32;
    int32_t val;
    int32_t expval;
    temp32 = 0;
    for (i = 0; i < len; i++) {
        val = xvec[i];
        
        if (in_scale > 0) {
            val >>= in_scale;
        }
        
        if (in_scale < 0) {
            val <<= -in_scale;
        }
        
        expval = FixedPointExp2Q10(val << 3);

        temp32 += expval;
    }
    
    for (i = 0; i < len; i++) {
        val = xvec[i];
        
        if (in_scale > 0) {
            val >>= in_scale;
        }
        
        if (in_scale < 0) {
            val <<= -in_scale;
        }
        
       
        expval = FixedPointExp2Q10(val << 3);

        val = (expval << QFIXEDPOINT) / temp32;
    
    
        if (val > MAX_WEIGHT) {
            val = MAX_WEIGHT;
        }
        
        if (val < -MAX_WEIGHT) {
            val = -MAX_WEIGHT;
        }
        
        xvec[i] = val;
        
    }
    
}


void tinytensor_linear(Weight_t * y, int8_t * out_scale, int32_t x,int8_t in_scale) {
    
    if (x > MAX_WEIGHT) {
        x = MAX_WEIGHT;
    }
    
    if (x < -MAX_WEIGHT) {
        x = -MAX_WEIGHT;
    }
    
    *y = x;
    *out_scale = in_scale;
}


void tinytensor_relu(Weight_t * y, int8_t * out_scale, int32_t x,int8_t in_scale) {
    x =  x < 0 ? 0 : x;
    
    if (x > MAX_WEIGHT) {
        x = MAX_WEIGHT;
    }
    
    *y = x;
    *out_scale = in_scale;

}




int8_t tiny_tensor_get_scaling(int32_t x) {
    int8_t i;
    
    if (x == 0) {
        return 0;
    }

    //find max scaling
    x = tinymath_abs_int32(x);
    
    for (i = 0; i < 8; i++) {
        if (( ((int16_t)x) << i) > MAX_WEIGHT/2) {
            break;
        }
    }
    
    return i;
    
}

inline int8_t tiny_tensor_get_descaling(int32_t x) {
    int8_t i;

    x = tinymath_abs_int32(x);
    
    for (i = 0; i < 31; i++) {
        if ((x >> i) <= MAX_WEIGHT) {
            break;
        }
    }
    
    return i;
    
}

int8_t tiny_tensor_compare_scaled_numbers(const Weight_t x1, const int8_t scale1, const Weight_t x2, const int8_t scale2) {
    int32_t xx1 = x1 << 16;
    int32_t xx2 = x2 << 16;
    
    xx1 = tinymath_abs_int32(xx1);
    xx2 = tinymath_abs_int32(xx2);
    
    if (scale1 > 0) {
        xx1 >>= scale1;
    }
    else {
        xx1 <<= -scale1;
    }
    
    if (scale2 > 0) {
        xx2 >>= scale2;
    }
    else {
        xx2 <<= -scale2;
    }
  
    
    if (xx1 > xx2) {
        return 1;
    }
    
    if (xx2 > xx1) {
        return -1;
    }
    
    return 0;
    
}



