#include "top_board.h"
#include "top_hci.h"
#include "slip_packet.h"
#include "rom_map.h"
#include <hw_types.h>
#include <uart.h>
#include <hw_memmap.h>
#include <prcm.h>
#include <stdint.h>
#include <string.h>
#include "fs.h"
#include "uartstdio.h"

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
//#include "led_cmd.h"
//#include "led_animations.h"
#include "uart_logger.h"
#include "stdlib.h"
#include "ble_proto.h"
#include "crypto.h"
#include "hlo_async.h"
#include "cmdline.h"
#include "hlo_pipe.h"

#define TOPBOARD_INFO_FILE "/top/info.bin"

#define UART_INTFLAGS (UART_INT_EOT | UART_INT_OE | UART_INT_RT | UART_INT_TX | UART_INT_RX)

typedef struct{
    uint8_t update_sha[SHA1_SIZE];
}top_info_t;

typedef enum {
	DFU_INVALID_PACKET = 0,
	DFU_INIT_PACKET = 1,
	DFU_START_DATA_PACKET = 2,
	DFU_DATA_PACKET = 3,
	DFU_STOP_DATA_PACKET = 4,
	DFU_IDLE = 0xff
} dfu_packet_type;

static struct{
	enum{
		TOP_NORMAL_MODE = 0,
		TOP_DFU_MODE
	}mode;
	dfu_packet_type dfu_state;
	hci_decode_handler_t hci_handler;
	struct{
		uint16_t crc;
		uint32_t len;
		uint32_t offset;
		long handle;

	}dfu_contex;
	volatile int top_boot;
    top_info_t info;
    TaskHandle_t top_board_task_hndl;
}self;

#define MAX_LINE_LENGTH 128
static char line[MAX_LINE_LENGTH] = {0};
static int linepos=0;

extern volatile bool booted;
static void
_printchar(uint8_t c){
    if( booted ) {
    	{
    		char s[2];
    		s[0] = c;
    		s[1] = 0;
    		LOGT(s);
    	}
    	line[linepos++] = c;
    	if( c == '\n' ) {
    		line[linepos] = 0;
    		if( line[0] == '_' && line[1] == ' ' ) {
				char * args = pvPortMalloc(linepos-2);
				memcpy(args, line+2, linepos-2);
				xTaskCreate(CmdLineProcess, "commandTask",  3*1024 / 4, args, 3, NULL);
    		} else if( match( "</data>", line ) ) {
    			LOGF(line+strlen("<data>"));
    			LOGF("\r\n");
    		}
    		memset(line, 0, sizeof(line));
    		linepos = 0;
    	}
    	if( linepos >= MAX_LINE_LENGTH ) {
    		memset(line, 0, sizeof(line));
    		linepos = 0;
    	}
    }
    /*
	 *UARTCharPutNonBlocking(UARTA0_BASE, c); //basic feedback
     */
#if UART_LOGGER_MODE == UART_LOGGER_MODE_RAW
	uart_logc(c);
#endif

}

int sf_sha1_verify(const char * sha_truth, const char * serial_file_path);
int verify_top_update(void);

static TimerHandle_t boot_timer;

int Cmd_SyncID(int argc, char * argv[]);
static void boot_check( TimerHandle_t pxTimer ) {
	Cmd_SyncID(0, 0);
}

static int32_t
_encode_and_send(uint8_t* orig, uint32_t size){
	uint32_t out_size, ret;
	uint8_t * msg = hci_encode(orig, size, &out_size);
	if(msg){
		ret = slip_write(msg,out_size);
		hci_free(msg);
		return ret;
	}
	return -1;
}
#define minval(a,b) ((a)<(b)?(a):(b))
static dfu_packet_type
_next_file_data_block(uint8_t * write_buf, uint32_t buffer_size, uint32_t * out_actual_size){
	int status;
	if( self.dfu_contex.len - self.dfu_contex.offset < buffer_size ) {
		buffer_size = self.dfu_contex.len - self.dfu_contex.offset;
	}
	status = sl_FsRead(self.dfu_contex.handle, self.dfu_contex.offset, write_buf, buffer_size );
	if(status > 0){
		self.dfu_contex.offset += status;
		*out_actual_size = status;
		return DFU_DATA_PACKET;
	}else{
		*out_actual_size = 0;
		return DFU_STOP_DATA_PACKET;
	}
}
static void
_on_message(uint8_t * message_body, uint32_t body_length){
	LOGI("Got a SLIP message: %s\r\n", message_body);
	if(!strncmp("DFUBEGIN",(char*)message_body, body_length)){
		//play_led_progress_bar(30,0,0,0, portMAX_DELAY);
		vTaskDelay(2000);
		if(0 != verify_top_update() || 0 != top_board_dfu_begin("/top/update.bin")){
			top_board_dfu_begin("/top/factory.bin");
		}
		//led_set_color(0xFF, 50,0,0,0,0,0,0);

	}
}

static void
_close_and_reset_dfu(){
	sl_FsClose(self.dfu_contex.handle, 0,0,0);
	self.mode = TOP_NORMAL_MODE;
}
static void
_on_ack_failed(void){
	_close_and_reset_dfu();
}
static void
_on_decode_failed(void){
	if(self.mode == TOP_DFU_MODE){
		_close_and_reset_dfu();
	}
}
bool is_top_in_dfu(void){
	return (self.mode == TOP_DFU_MODE);
}
static void
_on_ack_success(void){
	//vTaskDelay(10);
	if (self.mode == TOP_DFU_MODE) {
		switch (self.dfu_state) {
		case DFU_INVALID_PACKET:
			{
			self.dfu_state = DFU_START_DATA_PACKET;
			uint32_t begin_packet[] = { DFU_START_DATA_PACKET, self.dfu_contex.len };
			_encode_and_send((uint8_t*) begin_packet, sizeof(begin_packet));
			}
			break;
		case DFU_START_DATA_PACKET:
			{
			self.dfu_state = DFU_INIT_PACKET;
			uint32_t init_packet[] = { (uint32_t) DFU_INIT_PACKET,
					(uint32_t) self.dfu_contex.crc };
			_encode_and_send((uint8_t*)init_packet, sizeof(init_packet));
			}
			break;
		case DFU_INIT_PACKET:
		case DFU_DATA_PACKET:
			{
			uint32_t block[128];
			uint32_t written;
			block[0] = DFU_DATA_PACKET;
			self.dfu_state = _next_file_data_block(
					((uint8_t*) block) + sizeof(uint32_t),
					(sizeof(block) - sizeof(uint32_t)), &written);
			if(written){
				_encode_and_send((uint8_t*)block, written + sizeof(block[0]));
				LOGI("Wrote %u / %d (%u)%%\r", self.dfu_contex.offset, self.dfu_contex.len, (self.dfu_contex.offset*100/self.dfu_contex.len));
				//set_led_progress_bar((self.dfu_contex.offset*100/self.dfu_contex.len));
			}else{
				uint32_t primer_packet[] = { DFU_INVALID_PACKET };
				_encode_and_send((uint8_t*) primer_packet,sizeof(primer_packet));
			}
			}
			break;
		case DFU_STOP_DATA_PACKET:
			{
			uint32_t end_packet[] = { DFU_STOP_DATA_PACKET };
			_encode_and_send((uint8_t*)end_packet, sizeof(end_packet));
			self.dfu_state = DFU_IDLE;
			LOGI("Attempting to boot top board...\r\n");
			//play_led_animation_solid(LED_MAX, 0,10,0,1,18);
			}
			break;
		default:
		case DFU_IDLE:
			_close_and_reset_dfu();
			break;
		}
	}
}
static void
_on_slip_message(uint8_t * c, uint32_t size){
	uint32_t err = hci_decode(c, size, &self.hci_handler);
}
static void
_on_dtm_event(uint16_t dtm_event){
	LOGI("Got a dtm event: %04X\r\n", dtm_event);
}

static void
_sendchar(uint8_t c){
    UARTCharPut(UARTA1_BASE, c);
}
static void
_top_uart_isr() {
	signed long xHigherPriorityTaskWoken;
	traceISR_ENTER();
	UARTIntClear( UARTA1_BASE, UART_INTFLAGS );

    vTaskNotifyGiveFromISR( self.top_board_task_hndl, &xHigherPriorityTaskWoken );
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	traceISR_EXIT();
}
void top_board_task(void * params){
	slip_handler_t me = {
			.slip_display_char = _printchar,
			.slip_on_message = _on_slip_message,
			.slip_put_char = _sendchar,
			.slip_on_dtm_event = _on_dtm_event
	};
	self.hci_handler = (hci_decode_handler_t){
			.on_message = _on_message,
			.on_ack_failed = _on_ack_failed,
			.on_ack_success = _on_ack_success,
			.on_decode_failed = _on_decode_failed
	};
	self.mode = TOP_NORMAL_MODE;
	slip_reset(&me);
	hci_init();

	self.top_board_task_hndl =  xTaskGetCurrentTaskHandle();

	MAP_UARTConfigSetExpClk(UARTA1_BASE, PRCMPeripheralClockGet(PRCM_UARTA1),
			38400,
			(UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
	UARTFIFOLevelSet( UARTA1_BASE, UART_FIFO_TX1_8, UART_FIFO_RX1_8 );
	UARTIntEnable( UARTA1_BASE, UART_INTFLAGS );
	UARTIntRegister(UARTA1_BASE, _top_uart_isr );
	while (1) {
		while( UARTCharsAvail(UARTA1_BASE)) {
			int8_t c = UARTCharGetNonBlocking(UARTA1_BASE);
			if( c != -1 ) {
				slip_handle_rx(c);
			}
		}
		ulTaskNotifyTake( pdTRUE, portMAX_DELAY );
	}
}
static int _prep_file(const char * name, uint32_t * out_fsize, uint16_t * out_crc, long * out_handle){
	if(!out_fsize || !out_crc){
		return -1;
	}
	unsigned char buffer[128] = {0};
	unsigned long tok = 0;
	long hndl = 0;
	uint16_t crc = 0xFFFFu;
	SlFsFileInfo_t info;
	sl_FsGetInfo((unsigned char*)name, tok, &info);
	if( (hndl = sl_FsOpen((unsigned char*)name, SL_FS_READ, NULL)) < 0){
		LOGI("error opening for read %s. %d\r\n", name, hndl);
		return hndl;
	}else{
		LOGI("Opened fw for top ota: %s.\r\n", name);
    }
	uint32_t bytes_to_read = info.Len;
	int32_t bytes;
	while(bytes_to_read){
		bytes = sl_FsRead(hndl,
				info.Len - bytes_to_read,
				buffer,
				min(sizeof(buffer), bytes_to_read));
		if ( bytes > 0){
			crc = hci_crc16_compute_cont(buffer, bytes, &crc);
		}else{
			LOGE("FSError %d\r\n", bytes);
			return -1;
		}
		bytes_to_read -= bytes;
	}
	//sl_FsClose(hndl, 0,0,0);
	*out_crc = crc;
	*out_fsize = info.Len;
	*out_handle = hndl;
	LOGI("Bytes Read %u, crc = %u.\r\n", *out_fsize, *out_crc);
	return 0;
}

#include "fs_utils.h"
static int
_load_top_info(top_info_t * info){
	memset(info, 0, sizeof(top_info_t));
    return fs_get( TOPBOARD_INFO_FILE, (void*)info, sizeof(top_info_t), NULL );
}
static int
_save_top_info(const top_info_t * info){
	int ret = -1;
	hlo_stream_t * fs = open_serial_flash(TOPBOARD_INFO_FILE, HLO_STREAM_WRITE, sizeof(top_info_t));
	if(fs){
		int ret = hlo_stream_transfer_all(INTO_STREAM,fs,info,sizeof(*info), 4);
		if(ret == sizeof(*info)){
			LOGI("Top Info Success\r\n");
			ret = 0;
		}else{
			LOGW("Top Info Failure %d\r\n", ret);
		}
		hlo_stream_close(fs);
	}
	return ret;
}
int top_board_dfu_begin(const char * bin){
	int ret;

	char * target = (char*)bin;
	if(!bin){
		return -1;
	}

	if(self.mode == TOP_NORMAL_MODE){
		self.mode = TOP_DFU_MODE;
		uint16_t crc;
		uint32_t len;
		long handle;
		ret = _prep_file(target ,&len, &crc, &handle);
		if(0 == ret){
			self.dfu_contex.crc = crc;
			self.dfu_contex.len = len;
			self.dfu_contex.offset = 0;
			self.dfu_contex.handle = handle;
			//Primer packet is to sync up ACK packets.
			uint32_t primer_packet[] = {DFU_INVALID_PACKET};
			_encode_and_send((uint8_t*) primer_packet, sizeof(primer_packet));
			self.dfu_state = DFU_INVALID_PACKET;
		}else{
            LOGE("Preparation of OTA file failed\r\n");
			self.mode = TOP_NORMAL_MODE;
			return ret;
		}
	}else{
		_close_and_reset_dfu();
		LOGI("Already in dfu mode, resetting context\r\n");
	}
	return 0;
}
int wait_for_top_boot(unsigned int timeout) {
	unsigned int start = xTaskGetTickCount();
	self.top_boot = false;
	while( !self.top_boot && xTaskGetTickCount() - start < timeout ) {
		vTaskDelay(100);
	}
	return self.top_boot;
}
int send_top(char * s, int n) {
	int i;

	if( !self.top_boot ) {
		return -1;
	}
	if(self.mode == TOP_NORMAL_MODE){
		if(memcmp(s, "r ", 2) != 0)
		{
			for (i = 0; i < n; i++) {
				UARTCharPut(UARTA1_BASE, *(s+i));
			}
			UARTCharPut(UARTA1_BASE, '\r');
			UARTCharPut(UARTA1_BASE, '\n');
		}else{
			UARTCharPut(UARTA1_BASE, *s);
			LOGI("recover send\n");
		}
		return 0;
	}else{
		LOGI("Top board is in DFU mode\r\n");
		return -1;
	}
}
#include "kit_assert.h"
int Cmd_send_top(int argc, char *argv[]){
	int ret,i;
	char * start;
	char * buf = pvPortMalloc(256);
	start = buf;
	assert(buf);
	for (i = 1; i < argc; i++) {
		int j = 0;
		while (argv[i][j] != '\0') {
			*buf++ = argv[i][j++];
		}
		*buf++ = ' ';
	}
	ret = send_top(start, buf - start);
	vPortFree(start);

	return ret;
}
void top_board_notify_boot_complete(void){
	self.top_boot = true;
	if( boot_timer ) {
		xTimerStop(boot_timer, portMAX_DELAY);
	}
}
void set_top_update_sha(const char * shasum, unsigned int imgnum){
    _load_top_info(&self.info);
    memcpy(self.info.update_sha, shasum, SHA1_SIZE);
    _save_top_info(&self.info);
}

#include "dtm.h"
int Cmd_top_dtm(int argc, char * argv[]){
	slip_dtm_mode();
	uint16_t slip_cmd = 0;
	if(argc > 1){
		if(!strcmp(argv[1], "end")){
			slip_cmd = DTM_CMD(DTM_CMD_TEST_END);
		}else if(!strcmp(argv[1], "reset")){
			slip_cmd = 0;//reset is all 0s
		}else if(!strcmp(argv[1], "code") && argc > 2){
			slip_cmd = (uint16_t)strtol(argv[2], NULL, 16);
			LOGI("Trying Slip Code 0x%02X (%d)\r\n", slip_cmd, slip_cmd);
		}else{
			return -1;
		}
	}else{
		return -1;
	}
	slip_write((uint8_t*)&slip_cmd,sizeof(slip_cmd));
	return 0;
}
int verify_top_update(void){
    _load_top_info(&self.info);
    return sf_sha1_verify((char*)self.info.update_sha, "/top/update.bin");
}

void start_top_boot_watcher(void){
	if( is_test_boot() ) {
		return;
	}
	boot_timer = xTimerCreate("Boot timer",10000,pdTRUE, 0, boot_check);
	if( boot_timer ) {
		xTimerStart( boot_timer, portMAX_DELAY );
	}
}
