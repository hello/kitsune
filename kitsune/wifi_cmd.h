#ifndef __WIFI_CMD_H__
#define __WIFI_CMD_H__

#include "endpoints.h"
#include "periodic.pb.h"
#include "morpheus_ble.pb.h"



#define PILL_MAGIC 0xAAAAAAAA
#include "FreeRTOS.h"
#include "semphr.h"
#include "wlan.h"
#include "ble_cmd.h"
#include "network_types.h"

#ifndef MAX_SSID_LEN
#define MAX_SSID_LEN	(32)
#endif

extern xSemaphoreHandle pill_smphr;

typedef struct {
	unsigned int time;
	int light, light_variability, light_tonality, temp, humid, dust, dust_max, dust_min, dust_var;
} data_t;

extern
int sl_mode;


#define CONNECT    0x00000001
#define HAS_IP     0x00000002
#define IP_LEASED  0x00000004
#define SCANNING   0x00000008
#define CONNECTING 0x00000010
#define UPLOADING  0x00000020

#define AES_KEY_LOC "/cert/key.aes"

#include "stdint.h"
#include "SyncResponse.pb.h"

#define INV_TIME 0xffffffff
extern
SyncResponse_Alarm alarm;

//todo semaphore protect
extern
unsigned int sl_status;

#define IFA_ANT 1
#define CHIP_ANT 2
void antsel(unsigned char a);
int Cmd_antsel(int argc, char *argv[]);

int Cmd_connect(int argc, char *argv[]);
int Cmd_disconnect(int argc, char *argv[]);
int Cmd_ping(int argc, char *argv[]);
int Cmd_status(int argc, char *argv[]);
int Cmd_audio_test(int argc, char *argv[]);
int Cmd_time(int argc, char*argv[]);
int Cmd_sl(int argc, char*argv[]);
int Cmd_mode(int argc, char*argv[]);
int Cmd_set_mac(int argc, char*argv[]);
int Cmd_set_aes(int argc, char *argv[]) ;


int Cmd_RadioStartRX(int argc, char*argv[]);
int Cmd_RadioStopRX(int argc, char*argv[]);
int Cmd_RadioStopTX(int argc, char*argv[]);
int Cmd_RadioStartTX(int argc, char*argv[]);
int Cmd_RadioGetStats(int argc, char*argv[]);
int Cmd_data_upload(int arg, char* argv[]);

bool get_mac(unsigned char mac[6]);

int match(char *regexp, char *text);
unsigned long unix_time();
void load_aes();


int send_periodic_data(periodic_data* data);
int send_audio_data( data_t * data );
int send_pill_data(batched_pill_data * pill_data);

void thread_ota( void * unused );


int send_data_pb_callback(const char* host, const char* path,char * recv_buf, uint32_t recv_buf_size,const void * encodedata,network_encode_callback_t encoder,uint16_t num_receive_retries);

int decode_rx_data_pb_callback(const uint8_t * buffer, uint32_t buffer_size, void * decodedata,network_decode_callback_t decoder);
int decode_rx_data_pb(const uint8_t * buffer, uint32_t buffer_size, const  pb_field_t fields[],void * structdata);


int http_response_ok(const char* response_buffer);

int get_wifi_scan_result(Sl_WlanNetworkEntry_t* entries, uint16_t entry_len, uint32_t scan_duration_ms);
int connect_scanned_endpoints(const char* ssid, const char* password, 
    const Sl_WlanNetworkEntry_t* wifi_endpoints, int scanned_wifi_count, SlSecParams_t* connectedEPSecParamsPtr);
int connect_wifi(const char* ssid, const char* password, int sec_type);
void wifi_get_connected_ssid(uint8_t* ssid_buffer, size_t len);

void wifi_reset();
void free_pill_list();


bool encode_mac(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
bool encode_mac_as_device_id_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);
bool encode_name(pb_ostream_t *stream, const pb_field_t *field, void * const *arg);

#define MORPH_NAME ""
#define KIT_VER 18

#endif
