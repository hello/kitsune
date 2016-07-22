#include "../tinytensor_lstm_layer.h"
#include "../tinytensor_conv_layer.h"
#include "../tinytensor_fullyconnected_layer.h"
#include "../tinytensor_math.h"
#include "../tinytensor_net.h"
const static uint32_t LSTM03_01_input_dims[4] = {1,1,157,5};
const static uint32_t LSTM03_01_output_dims[4] = {1,1,157,4};
const static Weight_t LSTM03_01_weights_input_gate_x[36] = {37,10,-28,-2,24,-24,-3,-61,-25,17,6,-42,-44,41,26,64,-11,-6,20,32,-18,52,-13,-49,24,0,45,-36,38,-29,19,28,36,-16,-33,48};
const static uint32_t LSTM03_01_weights_input_gate_dims[4] = {1,1,4,9};
const static ConstTensor_t LSTM03_01_weights_input_gate = {&LSTM03_01_weights_input_gate_x[0],&LSTM03_01_weights_input_gate_dims[0],-1};

const static Weight_t LSTM03_01_biases_input_gate_x[4] = {0,0,0,0};
const static uint32_t LSTM03_01_biases_input_gate_dims[4] = {1,1,1,4};
const static ConstTensor_t LSTM03_01_biases_input_gate = {&LSTM03_01_biases_input_gate_x[0],&LSTM03_01_biases_input_gate_dims[0],0};

const static Weight_t LSTM03_01_weights_cell_x[36] = {45,97,55,43,-58,-107,8,-91,-9,-83,68,-92,-12,40,87,-31,-104,-22,-16,12,-32,37,48,18,117,-4,-76,78,20,-33,28,-66,20,71,-29,116};
const static uint32_t LSTM03_01_weights_cell_dims[4] = {1,1,4,9};
const static ConstTensor_t LSTM03_01_weights_cell = {&LSTM03_01_weights_cell_x[0],&LSTM03_01_weights_cell_dims[0],0};

const static Weight_t LSTM03_01_biases_cell_x[4] = {0,0,0,0};
const static uint32_t LSTM03_01_biases_cell_dims[4] = {1,1,1,4};
const static ConstTensor_t LSTM03_01_biases_cell = {&LSTM03_01_biases_cell_x[0],&LSTM03_01_biases_cell_dims[0],0};

const static Weight_t LSTM03_01_weights_forget_gate_x[36] = {47,-45,-4,-13,-7,25,23,56,-26,1,-40,-33,9,-37,-61,25,22,11,-43,-8,34,-35,47,-21,-11,-16,-64,50,47,25,45,-28,12,61,-33,-6};
const static uint32_t LSTM03_01_weights_forget_gate_dims[4] = {1,1,4,9};
const static ConstTensor_t LSTM03_01_weights_forget_gate = {&LSTM03_01_weights_forget_gate_x[0],&LSTM03_01_weights_forget_gate_dims[0],-1};

const static Weight_t LSTM03_01_biases_forget_gate_x[4] = {64,64,64,64};
const static uint32_t LSTM03_01_biases_forget_gate_dims[4] = {1,1,1,4};
const static ConstTensor_t LSTM03_01_biases_forget_gate = {&LSTM03_01_biases_forget_gate_x[0],&LSTM03_01_biases_forget_gate_dims[0],-1};

const static Weight_t LSTM03_01_weights_output_gate_x[36] = {94,-35,39,89,49,-69,-87,-21,84,-102,-35,59,-78,-25,104,-68,59,30,62,-37,34,82,-75,-36,-80,23,-107,-89,65,53,-70,84,55,-33,-124,-20};
const static uint32_t LSTM03_01_weights_output_gate_dims[4] = {1,1,4,9};
const static ConstTensor_t LSTM03_01_weights_output_gate = {&LSTM03_01_weights_output_gate_x[0],&LSTM03_01_weights_output_gate_dims[0],0};

const static Weight_t LSTM03_01_biases_output_gate_x[4] = {0,0,0,0};
const static uint32_t LSTM03_01_biases_output_gate_dims[4] = {1,1,1,4};
const static ConstTensor_t LSTM03_01_biases_output_gate = {&LSTM03_01_biases_output_gate_x[0],&LSTM03_01_biases_output_gate_dims[0],0};

const static LstmLayer_t LSTM03_01 = {&LSTM03_01_weights_input_gate,&LSTM03_01_biases_input_gate,&LSTM03_01_weights_cell,&LSTM03_01_biases_cell,&LSTM03_01_weights_forget_gate,&LSTM03_01_biases_forget_gate,&LSTM03_01_weights_output_gate,&LSTM03_01_biases_output_gate,LSTM03_01_output_dims,LSTM03_01_input_dims,TOFIX(0.000000),tinytensor_tanh};


const static uint32_t LSTM03_02_input_dims[4] = {1,1,157,4};
const static uint32_t LSTM03_02_output_dims[4] = {1,1,157,3};
const static Weight_t LSTM03_02_weights_input_gate_x[21] = {-109,20,46,100,-122,19,67,-82,-16,115,56,38,-95,97,-33,-60,-60,105,58,102,77};
const static uint32_t LSTM03_02_weights_input_gate_dims[4] = {1,1,3,7};
const static ConstTensor_t LSTM03_02_weights_input_gate = {&LSTM03_02_weights_input_gate_x[0],&LSTM03_02_weights_input_gate_dims[0],0};

const static Weight_t LSTM03_02_biases_input_gate_x[3] = {0,0,0};
const static uint32_t LSTM03_02_biases_input_gate_dims[4] = {1,1,1,3};
const static ConstTensor_t LSTM03_02_biases_input_gate = {&LSTM03_02_biases_input_gate_x[0],&LSTM03_02_biases_input_gate_dims[0],0};

const static Weight_t LSTM03_02_weights_cell_x[21] = {-27,11,-14,52,-17,66,-19,31,-59,15,-2,-65,-21,-17,5,15,-30,48,-21,13,66};
const static uint32_t LSTM03_02_weights_cell_dims[4] = {1,1,3,7};
const static ConstTensor_t LSTM03_02_weights_cell = {&LSTM03_02_weights_cell_x[0],&LSTM03_02_weights_cell_dims[0],-1};

const static Weight_t LSTM03_02_biases_cell_x[3] = {0,0,0};
const static uint32_t LSTM03_02_biases_cell_dims[4] = {1,1,1,3};
const static ConstTensor_t LSTM03_02_biases_cell = {&LSTM03_02_biases_cell_x[0],&LSTM03_02_biases_cell_dims[0],0};

const static Weight_t LSTM03_02_weights_forget_gate_x[21] = {14,12,22,19,55,18,40,55,52,8,5,-1,65,-28,14,-37,-11,38,-44,21,50};
const static uint32_t LSTM03_02_weights_forget_gate_dims[4] = {1,1,3,7};
const static ConstTensor_t LSTM03_02_weights_forget_gate = {&LSTM03_02_weights_forget_gate_x[0],&LSTM03_02_weights_forget_gate_dims[0],-1};

const static Weight_t LSTM03_02_biases_forget_gate_x[3] = {64,64,64};
const static uint32_t LSTM03_02_biases_forget_gate_dims[4] = {1,1,1,3};
const static ConstTensor_t LSTM03_02_biases_forget_gate = {&LSTM03_02_biases_forget_gate_x[0],&LSTM03_02_biases_forget_gate_dims[0],-1};

const static Weight_t LSTM03_02_weights_output_gate_x[21] = {24,-42,20,42,-48,-22,47,28,-46,-34,-42,42,20,53,47,26,4,-28,-30,64,0};
const static uint32_t LSTM03_02_weights_output_gate_dims[4] = {1,1,3,7};
const static ConstTensor_t LSTM03_02_weights_output_gate = {&LSTM03_02_weights_output_gate_x[0],&LSTM03_02_weights_output_gate_dims[0],-1};

const static Weight_t LSTM03_02_biases_output_gate_x[3] = {0,0,0};
const static uint32_t LSTM03_02_biases_output_gate_dims[4] = {1,1,1,3};
const static ConstTensor_t LSTM03_02_biases_output_gate = {&LSTM03_02_biases_output_gate_x[0],&LSTM03_02_biases_output_gate_dims[0],0};

const static LstmLayer_t LSTM03_02 = {&LSTM03_02_weights_input_gate,&LSTM03_02_biases_input_gate,&LSTM03_02_weights_cell,&LSTM03_02_biases_cell,&LSTM03_02_weights_forget_gate,&LSTM03_02_biases_forget_gate,&LSTM03_02_weights_output_gate,&LSTM03_02_biases_output_gate,LSTM03_02_output_dims,LSTM03_02_input_dims,TOFIX(0.000000),tinytensor_tanh};





static ConstLayer_t _layers03[2];
static ConstSequentialNetwork_t net3 = {&_layers03[0],2};
static ConstSequentialNetwork_t initialize_network03(void) {

  _layers03[0] = tinytensor_create_lstm_layer(&LSTM03_01);
  _layers03[1] = tinytensor_create_lstm_layer(&LSTM03_02);
  return net3;

}
