#include "../tinytensor_lstm_layer.h"
#include "../tinytensor_conv_layer.h"
#include "../tinytensor_fullyconnected_layer.h"
#include "../tinytensor_math.h"
#include "../tinytensor_net.h"
const static uint32_t LSTM2_01_input_dims[4] = {1,1,199,5};
const static uint32_t LSTM2_01_output_dims[4] = {1,1,199,4};
const static Weight_t LSTM2_01_weights_input_gate_x[36] = {81,17,82,2,103,-95,43,-84,-44,22,60,9,46,-82,-94,-86,59,6,-77,68,56,20,101,44,-90,-52,-84,-77,66,23,51,25,1,-50,-81,104};
const static uint32_t LSTM2_01_weights_input_gate_dims[4] = {1,1,4,9};
const static ConstTensor_t LSTM2_01_weights_input_gate = {&LSTM2_01_weights_input_gate_x[0],&LSTM2_01_weights_input_gate_dims[0],0};

const static Weight_t LSTM2_01_biases_input_gate_x[4] = {0,0,0,0};
const static uint32_t LSTM2_01_biases_input_gate_dims[4] = {1,1,1,4};
const static ConstTensor_t LSTM2_01_biases_input_gate = {&LSTM2_01_biases_input_gate_x[0],&LSTM2_01_biases_input_gate_dims[0],0};

const static Weight_t LSTM2_01_weights_cell_x[36] = {30,34,-31,36,-48,-19,-26,39,-49,3,12,-37,-29,23,8,-16,50,46,51,23,47,51,-25,-21,-58,-30,16,-31,32,19,10,41,-64,25,4,15};
const static uint32_t LSTM2_01_weights_cell_dims[4] = {1,1,4,9};
const static ConstTensor_t LSTM2_01_weights_cell = {&LSTM2_01_weights_cell_x[0],&LSTM2_01_weights_cell_dims[0],-1};

const static Weight_t LSTM2_01_biases_cell_x[4] = {0,0,0,0};
const static uint32_t LSTM2_01_biases_cell_dims[4] = {1,1,1,4};
const static ConstTensor_t LSTM2_01_biases_cell = {&LSTM2_01_biases_cell_x[0],&LSTM2_01_biases_cell_dims[0],0};

const static Weight_t LSTM2_01_weights_forget_gate_x[36] = {-27,-46,-52,37,-48,-56,15,12,-37,-9,4,-44,13,33,-13,-8,-69,-5,36,27,31,20,-24,-23,43,-4,51,32,-38,3,-37,-48,-33,-53,10,31};
const static uint32_t LSTM2_01_weights_forget_gate_dims[4] = {1,1,4,9};
const static ConstTensor_t LSTM2_01_weights_forget_gate = {&LSTM2_01_weights_forget_gate_x[0],&LSTM2_01_weights_forget_gate_dims[0],-1};

const static Weight_t LSTM2_01_biases_forget_gate_x[4] = {64,64,64,64};
const static uint32_t LSTM2_01_biases_forget_gate_dims[4] = {1,1,1,4};
const static ConstTensor_t LSTM2_01_biases_forget_gate = {&LSTM2_01_biases_forget_gate_x[0],&LSTM2_01_biases_forget_gate_dims[0],-1};

const static Weight_t LSTM2_01_weights_output_gate_x[36] = {30,49,9,-41,33,-31,-43,-17,43,21,14,40,-44,35,-13,25,-64,-10,-35,40,-2,-1,36,-59,3,19,-33,6,52,-50,13,-19,-17,50,16,43};
const static uint32_t LSTM2_01_weights_output_gate_dims[4] = {1,1,4,9};
const static ConstTensor_t LSTM2_01_weights_output_gate = {&LSTM2_01_weights_output_gate_x[0],&LSTM2_01_weights_output_gate_dims[0],-1};

const static Weight_t LSTM2_01_biases_output_gate_x[4] = {0,0,0,0};
const static uint32_t LSTM2_01_biases_output_gate_dims[4] = {1,1,1,4};
const static ConstTensor_t LSTM2_01_biases_output_gate = {&LSTM2_01_biases_output_gate_x[0],&LSTM2_01_biases_output_gate_dims[0],0};

const static LstmLayer_t LSTM2_01 = {&LSTM2_01_weights_input_gate,&LSTM2_01_biases_input_gate,&LSTM2_01_weights_cell,&LSTM2_01_biases_cell,&LSTM2_01_weights_forget_gate,&LSTM2_01_biases_forget_gate,&LSTM2_01_weights_output_gate,&LSTM2_01_biases_output_gate,LSTM2_01_output_dims,LSTM2_01_input_dims,TOFIX(0.000000),tinytensor_tanh};



