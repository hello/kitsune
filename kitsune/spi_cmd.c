#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "kit_assert.h"
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

#include "sl_sync_include_after_simplelink_header.h"

#if 0
#define SPI_DEBUG_PRINT
#endif

#define FAILURE                 -1
#define SUCCESS                 0

typedef struct {
	unsigned short len;
	unsigned short addr;
} ctx_t;

static xSemaphoreHandle _spi_smphr;
static int hw_ver;

#define READ 0
#define WRITE 1

#define SPI_IF_BIT_RATE  10000
#define TR_BUFF_SIZE     100

#include "hw_ver.h"
void CS_set(int val) {
	switch( hw_ver ) {
	case DVT:
	case PVT:
		MAP_GPIOPinWrite(GPIOA2_BASE,0x40,val?0x40:0);
		break;
	case EVT1_1p5:
		MAP_GPIOPinWrite(GPIOA2_BASE,0x40,val?0x40:0);
		break;
	default:
		LOGE("HW Version Not Supported\r\n");
		break;
	}
}

void spi_init() {
      hw_ver = get_hw_ver();
      vSemaphoreCreateBinary(_spi_smphr);
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

	xSemaphoreTake(_spi_smphr, portMAX_DELAY);
	CS_set(0);
	vTaskDelay(8*2);
//	MAP_SPITransfer(GSPI_BASE,&reset,&reset,1,SPI_CS_ENABLE|SPI_CS_DISABLE);
	MAP_SPITransfer(GSPI_BASE,&reset,&reset,1,0);

	CS_set(1);
	vTaskDelay(8*2);
	reset = 0xFF;
	CS_set(0);
	vTaskDelay(8*2);
//	MAP_SPITransfer(GSPI_BASE,&reset,&reset,1,SPI_CS_ENABLE|SPI_CS_DISABLE);
	MAP_SPITransfer(GSPI_BASE,&reset,&reset,1,0);

	CS_set(1);
	vTaskDelay(8*2);
	reset = 0xFF;
	CS_set(0);
	vTaskDelay(8*2);
//	MAP_SPITransfer(GSPI_BASE,&reset,&reset,1,SPI_CS_ENABLE|SPI_CS_DISABLE);
	MAP_SPITransfer(GSPI_BASE,&reset,&reset,1,0);

	CS_set(1);
	vTaskDelay(8*2);

	xSemaphoreGive(_spi_smphr);
	return SUCCESS;
}

int spi_write_step( int len, unsigned char * buf ) {
	int i;
	unsigned long dud;

	if( len > 256 ) {
		LOGI("Length limited to 256\r\n");
		return FAILURE;
	}
	//MAP_SPICSEnable(GSPI_BASE);

	CS_set(0);
	vTaskDelay(8*2);
	for (i = 0; i < len; i++) {
		MAP_SPIDataPut(GSPI_BASE, buf[i]);
		MAP_SPIDataGet(GSPI_BASE, &dud);
#ifdef SPI_DEBUG_PRINT
		LOGI("%x ", buf[i]);
#endif
	}
	//MAP_SPICSDisable(GSPI_BASE);
	CS_set(1);
	vTaskDelay(8*2);
#ifdef SPI_DEBUG_PRINT
	LOGI("\r\n");
#endif

	return SUCCESS;
}

int spi_read_step( int len, unsigned char * buf ) {
	if( len > 256 ) {
		LOGI("Length limited to 256\r\n");
		return FAILURE;
	}
#ifdef SPI_DEBUG_PRINT
	LOGI("Reading...\r\n");
#endif
	//	MAP_SPITransfer(GSPI_BASE,rx_buf,rx_buf,len,SPI_CS_ENABLE|SPI_CS_DISABLE);
	CS_set(0);
	vTaskDelay(8*2);
	len = MAP_SPITransfer(GSPI_BASE, buf, buf, len, 0);
	CS_set(1);
	vTaskDelay(8*2);
#ifdef SPI_DEBUG_PRINT
	LOGI("Read %d bytes \r\n", len);
	int i;
	for (i = 0; i < len; i++) {
		LOGI("%x ", buf[i]);
	}
	LOGI("\r\n");

#endif

	return SUCCESS;
}
int spi_write( int len, unsigned char * buf ) {
	unsigned char mode = WRITE;
	ctx_t ctx;

	xSemaphoreTake(_spi_smphr, portMAX_DELAY);
	spi_write_step( 1, &mode );
	vTaskDelay(8*2);
	ctx.len = len;
	ctx.addr = 0xcc;
	spi_write_step( 4, (unsigned char*)&ctx );
	vTaskDelay(8*2);
#ifdef SPI_DEBUG_PRINT
	LOGI("Ctx len %u, address %u\r\n",ctx.len, ctx.addr);
#endif
	assert( len < 256 );
	spi_write_step( len, buf );
	vTaskDelay(8*2);
	xSemaphoreGive(_spi_smphr);

	return SUCCESS;
}
int spi_read( int * len, unsigned char ** buf, int * addr ) {
	unsigned char mode = READ;
	ctx_t ctx;
	int i;

	xSemaphoreTake(_spi_smphr, portMAX_DELAY);
	spi_write_step( 1, &mode );
	vTaskDelay(8*2);
	spi_read_step( 4,  (unsigned char*)&ctx );
	vTaskDelay(8*2);
#ifdef SPI_DEBUG_PRINT
	LOGI("Ctx len %u, address %u\r\n",ctx.len, ctx.addr);
#endif
	if( ctx.addr == 0xAAAA || ctx.addr == 0x5500 || ctx.addr == 0x5555 || ctx.len > 256 ) {
		xSemaphoreGive(_spi_smphr );
		spi_reset();
		xSemaphoreTake(_spi_smphr, portMAX_DELAY);
		ctx.len = 0;
	}
	*addr = ctx.addr;
	*len = ctx.len;

	if( *len != 0 ) {
		*buf = (unsigned char*)pvPortMalloc( *len + 1 );
		spi_read_step(ctx.len, *buf);

		for(i=1;i<ctx.len;++i) {
			if((*buf)[i]!= 0x55 ) {
				break;
			}
		}
		if(i==ctx.len) {
			xSemaphoreGive(_spi_smphr );
			spi_reset();
			xSemaphoreTake(_spi_smphr, portMAX_DELAY);
		}
	}else{
		spi_read_step(0, *buf);
	}
	vTaskDelay(8*2);

	xSemaphoreGive(_spi_smphr );
	return SUCCESS;
}


int Cmd_spi_reset(int argc, char *argv[]) {
	return spi_reset();
}

int Cmd_spi_write(int argc, char *argv[]) {
	unsigned char len;

	if (argc != 2) {
		LOGI(
				"write  <rdlen> \n\r\t - Read data frpm the specified i2c device\n\r");
		return FAILURE;
	}
	len = strlen(argv[1]);

	LOGI("Writing...\r\n");
	spi_write( len, (unsigned char*)argv[1] );

	return SUCCESS;
}

extern volatile bool booted;
extern void CmdLineProcess(void * line);
int Cmd_spi_read(int argc, char *argv[]) {
#define MAX_PB_QUEUED 32
#define SPI_BUFSZ 512
	int len, addr = 0;
	unsigned char * buf;

	spi_read( &len, &buf, &addr );
#ifdef SPI_DEBUG_PRINT
	LOGI("read %u\r\n", len);
	int i;
	for( i=0; i<len; ++i ) {
		LOGI( "%x", buf[i] );
	}
	LOGI( "\r\n" );
#endif

	if( len ) {
		switch(addr){
		case 0:
			on_morpheus_protobuf_arrival(buf, len);
			break;
		case 0x02://command from the top!
		{
			char * args = pvPortMalloc(len);
			memcpy(args, buf, len);
			xTaskCreate(CmdLineProcess, "commandTask",  3*1024 / 4, args, 3, NULL);
		}
			break;
		default:
			DISP("Unkown SPI Address: %x\r\n", addr);
			break;
		}
		vPortFree(buf);
	}
	return SUCCESS;
}
