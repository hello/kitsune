#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <assert.h>
#include <stdint.h>

#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_common_reg.h"
#include "hw_ints.h"
#include "spi.h"
#include "pin.h"
#include "rom.h"
#include "rom_map.h"
#include "prcm.h"

#include "gpio_if.h"
#include "ble_cmd.h"

/* FreeRTOS includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "spi_cmd.h"
#include "uartstdio.h"

#if 0
#define SPI_DEBUG_PRINT
#endif

#define FAILURE                 -1
#define SUCCESS                 0

typedef struct {
	unsigned short len;
	unsigned short addr;
} ctx_t;

#define READ 0
#define WRITE 1

#define SPI_IF_BIT_RATE  10000
#define TR_BUFF_SIZE     100

void CS_set(int val) {
	  MAP_GPIOPinWrite(GPIOA1_BASE,0x20,val?0x20:0);
}

void spi_init() {
	  //
	  // Reset SPI
	  //
	  MAP_SPIReset(GSPI_BASE);
	  CS_set(1);
	  //
	  // Configure SPI interface
	  //
	  /*
	  MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
	                     SPI_IF_BIT_RATE,SPI_MODE_MASTER,SPI_SUB_MODE_0,
	                     (SPI_SW_CTRL_CS |
	                     SPI_4PIN_MODE |
	                     SPI_TURBO_OFF |
	                     SPI_CS_ACTIVELOW |
	                     SPI_WL_8));*/
	  MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
	                     SPI_IF_BIT_RATE,SPI_MODE_MASTER,SPI_SUB_MODE_0,
	                     ( SPI_3PIN_MODE |
	                     SPI_TURBO_OFF |
	                     SPI_WL_8));

	  //
	  // Enable SPI for communication
	  //
	  MAP_SPIEnable(GSPI_BASE);

}
int spi_reset(){
	unsigned char reset = 0xFF;
	CS_set(0);
	vTaskDelay(8*10);
//	MAP_SPITransfer(GSPI_BASE,&reset,&reset,1,SPI_CS_ENABLE|SPI_CS_DISABLE);
	MAP_SPITransfer(GSPI_BASE,&reset,&reset,1,0);

	CS_set(1);
	vTaskDelay(8*10);
	reset = 0xFF;
	CS_set(0);
	vTaskDelay(8*10);
//	MAP_SPITransfer(GSPI_BASE,&reset,&reset,1,SPI_CS_ENABLE|SPI_CS_DISABLE);
	MAP_SPITransfer(GSPI_BASE,&reset,&reset,1,0);

	CS_set(1);
	vTaskDelay(8*10);
	reset = 0xFF;
	CS_set(0);
	vTaskDelay(8*10);
//	MAP_SPITransfer(GSPI_BASE,&reset,&reset,1,SPI_CS_ENABLE|SPI_CS_DISABLE);
	MAP_SPITransfer(GSPI_BASE,&reset,&reset,1,0);

	CS_set(1);
	vTaskDelay(8*5);

	return SUCCESS;
}

int spi_write_step( int len, unsigned char * buf ) {
	int i;
	unsigned long dud;

	if( len > 256 ) {
		UARTprintf("Length limited to 256\r\n");
		return FAILURE;
	}
	//MAP_SPICSEnable(GSPI_BASE);
	CS_set(0);
	vTaskDelay(8*10);
	for (i = 0; i < len; i++) {
		MAP_SPIDataPut(GSPI_BASE, buf[i]);
		MAP_SPIDataGet(GSPI_BASE, &dud);
#ifdef SPI_DEBUG_PRINT
		UARTprintf("%x ", buf[i]);
#endif
	}
	//MAP_SPICSDisable(GSPI_BASE);
	CS_set(1);
	vTaskDelay(8*5);
#ifdef SPI_DEBUG_PRINT
	UARTprintf("\r\n");
#endif
	return SUCCESS;
}

int spi_read_step( int len, unsigned char * buf ) {
	if( len > 256 ) {
		UARTprintf("Length limited to 256\r\n");
		return FAILURE;
	}
#ifdef SPI_DEBUG_PRINT
	UARTprintf("Reading...\r\n");
#endif
	//	MAP_SPITransfer(GSPI_BASE,rx_buf,rx_buf,len,SPI_CS_ENABLE|SPI_CS_DISABLE);
	CS_set(0);
	vTaskDelay(8*10);
	len = MAP_SPITransfer(GSPI_BASE, buf, buf, len, 0);
	CS_set(1);
	vTaskDelay(8*5);
#ifdef SPI_DEBUG_PRINT
	UARTprintf("Read %d bytes \r\n", len);
	int i;
	for (i = 0; i < len; i++) {
		UARTprintf("%x ", buf[i]);
	}
	UARTprintf("\r\n");

#endif

	return SUCCESS;
}
int spi_write( int len, unsigned char * buf ) {
	unsigned char mode = WRITE;
	ctx_t ctx;

	spi_write_step( 1, &mode );
	vTaskDelay(8*10);
	ctx.len = len;
	ctx.addr = 0xcc;
	spi_write_step( 4, (unsigned char*)&ctx );
	vTaskDelay(8*10);
#ifdef SPI_DEBUG_PRINT
	UARTprintf("Ctx len %u, address %u\r\n",ctx.len, ctx.addr);
#endif
	spi_write_step( len, buf );
	vTaskDelay(8*10);

	return SUCCESS;
}
int spi_read( int * len, unsigned char * buf ) {
	unsigned char mode = READ;
	ctx_t ctx;
	int i;

	spi_write_step( 1, &mode );
	vTaskDelay(8*10);
	spi_read_step( 4,  (unsigned char*)&ctx );
	vTaskDelay(8*10);
#ifdef SPI_DEBUG_PRINT
	UARTprintf("Ctx len %u, address %u\r\n",ctx.len, ctx.addr);
#endif
	if( ctx.addr == 0xAAAA || ctx.addr == 0x5500 || ctx.addr == 0x5555 ) {
		spi_reset();
		ctx.len = 0;
	}
	*len = ctx.len;

	if( *len != 0 ) {
		spi_read_step(ctx.len, buf);

		for(i=1;i<ctx.len;++i) {
			if(buf[i]!= 0x55 ) {
				break;
			}
		}
		if(i==ctx.len) {
			spi_reset();
		}
	}else{
		spi_read_step(0, buf);
	}
	vTaskDelay(8*10);
	return SUCCESS;
}


int Cmd_spi_reset(int argc, char *argv[]) {
	return spi_reset();
}

int Cmd_spi_write(int argc, char *argv[]) {
	unsigned char len;

	if (argc != 2) {
		UARTprintf(
				"write  <rdlen> \n\r\t - Read data frpm the specified i2c device\n\r");
		return FAILURE;
	}
	len = strlen(argv[1]);

	UARTprintf("Writing...\r\n");
	spi_write( len, (unsigned char*)argv[1] );

	return SUCCESS;
}

int Cmd_spi_read(int argc, char *argv[]) {
	int len;
	unsigned char buf[256];

	spi_read( &len, buf );
#ifdef SPI_DEBUG_PRINT
	UARTprintf("read %u\r\n", len);
	int i;
	for( i=0; i<len; ++i ) {
		UARTprintf( "%x", buf[i] );
	}
	UARTprintf( "\r\n" );
#endif

	if( len ) {
	    on_morpheus_protobuf_arrival(buf, len);
	}

	return SUCCESS;

}
