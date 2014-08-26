#ifndef __I2C_CMD_H__
#define __I2C_CMD_H__

int Cmd_i2c_read(int argc, char *argv[]);
int Cmd_i2c_write(int argc, char *argv[]);
int Cmd_i2c_readreg(int argc, char *argv[]);
int Cmd_i2c_writereg(int argc, char *argv[]);

void Cmd_james();

int Cmd_readtemp(int argc, char *argv[]);
int Cmd_readhumid(int argc, char *argv[]);
int Cmd_readlight(int argc, char *argv[]);
int Cmd_readproximity(int argc, char *argv[]);
int get_codec_NAU();
//int get_codec_mic_NAU();

int get_temp();
int get_humid();
int get_light();
int get_prox();

#endif
