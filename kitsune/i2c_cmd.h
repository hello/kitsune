#ifndef __I2C_CMD_H__
#define __I2C_CMD_H__

int Cmd_i2c_read(int argc, char *argv[]);
int Cmd_i2c_write(int argc, char *argv[]);
int Cmd_i2c_readreg(int argc, char *argv[]);
int Cmd_i2c_writereg(int argc, char *argv[]);

int Cmd_readtemp(int argc, char *argv[]);

#endif
