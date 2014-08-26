#ifndef __SPI_CMD_H__
#define __SPI_CMD_H__

void spi_init();
int Cmd_spi_read(int argc, char *argv[]);
int Cmd_spi_write(int argc, char *argv[]);
int Cmd_spi_reset(int argc, char *argv[]);

#endif
