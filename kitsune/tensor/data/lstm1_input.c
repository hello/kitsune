




const static Weight_t lstm1_zeros_x[40] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
const static Weight_t lstm1_input_x[40] = {83,45,30,81,72,40,91,6,70,111,50,89,8,20,31,61,4,21,5,70,122,11,89,97,80,106,85,66,32,123,120,5,70,79,66,115,48,90,6,51};
const static uint32_t lstm1_input_dims[4] = {1,1,8,5};
const static ConstTensor_t lstm1_input = {&lstm1_input_x[0],&lstm1_input_dims[0],0};
const static ConstTensor_t lstm1_input_zeros = {&lstm1_zeros_x[0],&lstm1_input_dims[0],0};

