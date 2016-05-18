#ifndef __I2C_CMD_H__
#define __I2C_CMD_H__

#include "stdbool.h"

int Cmd_i2c_read(int argc, char *argv[]);
int Cmd_i2c_write(int argc, char *argv[]);
int Cmd_i2c_readreg(int argc, char *argv[]);
int Cmd_i2c_writereg(int argc, char *argv[]);

int Cmd_read_temp_hum_press(int argc, char *argv[]);
int Cmd_meas_TVOC(int argc, char *argv[]);

int Cmd_readlight(int argc, char *argv[]);
int Cmd_readproximity(int argc, char *argv[]);
//int get_codec_NAU();
int get_codec_NAU(int vol);
int close_codec_NAU();
int get_codec_mic_NAU();
bool set_volume(int v, unsigned int dly) ;

int get_temp_press_hum(int32_t * temp, uint32_t * press, uint32_t * hum);
int get_light();
uint32_t get_prox();

int init_temp_sensor();
int init_light_sensor();
int init_prox_sensor();

#endif
