/*
 * dust_cmd.h
 *
 *  Created on: Jun 30, 2014
 *      Author: chrisjohnson
 */

#ifndef DUST_CMD_H_
#define DUST_CMD_H_

#define DUST_SENSOR_NOT_READY ((unsigned)-1)

unsigned int get_dust();
int Cmd_dusttest(int argc, char *argv[]);

#endif /* ADC_CMD_H_ */
