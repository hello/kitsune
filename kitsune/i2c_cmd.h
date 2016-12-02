#ifndef __I2C_CMD_H__
#define __I2C_CMD_H__

#include "stdbool.h"

int Cmd_i2c_read(int argc, char *argv[]);
int Cmd_i2c_write(int argc, char *argv[]);
int Cmd_i2c_readreg(int argc, char *argv[]);
int Cmd_i2c_writereg(int argc, char *argv[]);

int Cmd_read_temp_humid_old(int argc, char *argv[]);
int Cmd_read_temp_hum_press(int argc, char *argv[]);
int Cmd_meas_TVOC(int argc, char *argv[]);

int Cmd_readlight(int argc, char *argv[]);
int Cmd_readproximity(int argc, char *argv[]);

int get_temp_press_hum(int32_t * temp, uint32_t * press, uint32_t * hum);

int get_ir( int * ir );

int get_rgb_prox( int * w, int * r, int * g, int * bl, int * p );
int get_tvoc(int * tvoc, int * eco2, int * current, int * voltage, int temp, unsigned int humid );

typedef enum {
	ZOPT_UV = 0,
	ZOPT_ALS = 1,
} zopt_mode;
int read_zopt(zopt_mode selection);
int Cmd_read_uv(int argc, char *argv[]);
int Cmd_uvr(int argc, char *argv[]);
int Cmd_uvw(int argc, char *argv[]);
int Cmd_set_tvenv(int argc, char * argv[]);
int cmd_tvoc_fw_update(int argc, char *argv[]);
int init_humid_sensor();
int init_temp_sensor();


typedef enum {
	HIGH_POWER,
	LOW_POWER,
} light_power_mode;
int light_sensor_power(light_power_mode power_state);
int init_light_sensor();
int init_tvoc(int measmode);

// AUDIO CODEC
#define CODEC_1P5_TEST

#ifdef CODEC_1P5_TEST
int32_t codec_test_commands(void);
#endif

int32_t codec_init(void);
void codec_unmute_spkr(void);
void codec_mute_spkr(void);

void codec_set_page(uint32_t page);
void codec_set_book(uint32_t book);
int32_t set_volume(int v, unsigned int dly);

int get_system_volume();
int32_t set_system_volume(int new_volume);

#endif
