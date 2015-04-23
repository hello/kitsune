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

extern
int sl_mode;


#define CONNECT    0x00000001
#define HAS_IP     0x00000002
#define IP_LEASED  0x00000004
#define SCANNING   0x00000008
#define CONNECTING 0x00000010
#define UPLOADING  0x00000020

#define AES_KEY_LOC "/cert/key.aes"
#define DEVICE_ID_LOC "/hello/deviceid"
#define DEVICE_ID_SZ 8

#define SERVER_REPLY_BUFSZ 32

#include "stdint.h"
#include "sync_response.pb.h"
#include "provision.pb.h"

#define INV_TIME 0xffffffff
extern
SyncResponse_Alarm alarm;


int Cmd_iperf_client(int argc, char *argv[]);
int Cmd_iperf_server(int argc, char *argv[]);
int Cmd_country(int argc, char *argv[]);


#define USER_DIR "/usr"
#define ANTENNA_FILE ("/hello/antenna")  // usr is in sd card, let's keep the original one.
#define ACCOUNT_ID_FILE "/hello/acct"
#define PROVISION_FILE "/pch/prov"
#define SERIAL_FILE "/pch/serial"

#define PCB_ANT 2
#define IFA_ANT 1
void save_default_antenna( unsigned char a );
unsigned char get_default_antenna();
int load_account_id();
void save_account_id( char * acct );
char * get_account_id();
void antsel(unsigned char a);
int Cmd_antsel(int argc, char *argv[]);

int Cmd_connect(int argc, char *argv[]);
int Cmd_disconnect(int argc, char *argv[]);
int Cmd_ping(int argc, char *argv[]);
int Cmd_status(int argc, char *argv[]);
int Cmd_time(int argc, char*argv[]);
int Cmd_sl(int argc, char*argv[]);
int Cmd_mode(int argc, char*argv[]);
int Cmd_set_mac(int argc, char*argv[]);
int Cmd_set_aes(int argc, char *argv[]) ;
int Cmd_test_key(int argc, char*argv[]);


int Cmd_RadioStartRX(int argc, char*argv[]);
int Cmd_RadioStopRX(int argc, char*argv[]);
int Cmd_RadioStopTX(int argc, char*argv[]);
int Cmd_RadioStartTX(int argc, char*argv[]);
int Cmd_RadioGetStats(int argc, char*argv[]);

bool get_mac(unsigned char mac[6]);
bool get_device_id(char * device_id, uint32_t size_of_device_id_buffer);

int match(char *regexp, char *text);
void load_aes();

void wifi_status_init();
int wifi_status_set(unsigned int status, int remove_status);
int wifi_status_get(unsigned int status);

bool send_periodic_data(batched_periodic_data* data);
bool send_pill_data(batched_pill_data * pill_data);
bool send_provision_request(ProvisionRequest* req);
#define DEFAULT_KEY "1234567891234567"

void thread_ota( void * unused );
int save_aes_in_memory(const uint8_t * key );
int get_aes(uint8_t * dst);
void on_key(uint8_t * key);
bool has_default_key();
bool should_burn_top_key();
int Cmd_burn_top(int argc, char *argv[]);

int send_data_pb_callback(const char* host, const char* path, char ** recv_buf_ptr,
		uint32_t * recv_buf_size_ptr, void * encodedata,
		network_encode_callback_t encoder, uint16_t num_receive_retries);

int decode_rx_data_pb_callback(const uint8_t * buffer, uint32_t buffer_size, void * decodedata,network_decode_callback_t decoder);
int decode_rx_data_pb(const uint8_t * buffer, uint32_t buffer_size, const  pb_field_t fields[],void * structdata);

bool validate_signatures( char * buffer, const pb_field_t fields[], void * structdata);

int http_response_ok(const char* response_buffer);

int get_wifi_scan_result(Sl_WlanNetworkEntry_t* entries, uint16_t entry_len, uint32_t scan_duration_ms, int antenna);
int connect_wifi(const char* ssid, const char* password, int sec_type, int version);
void wifi_get_connected_ssid(uint8_t* ssid_buffer, size_t len);

long nwp_reset();
void wifi_reset();
void free_pill_list();
void reset_default_antenna();


#define MORPH_NAME ""

void telnetServerTask(void *);
void httpServerTask(void *);

#endif
