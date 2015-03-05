/*
 * dust_cmd.h
 *
 *  Created on: Jun 30, 2014
 *      Author: chrisjohnson
 */

#ifndef DUST_CMD_H_
#define DUST_CMD_H_
#define DUST_SENSOR_NOT_READY		(-1)  // Assume dust output is positive

int get_dust();
int get_dust_internal(unsigned int samples);
int Cmd_dusttest(int argc, char *argv[]);

#endif /* ADC_CMD_H_ */
