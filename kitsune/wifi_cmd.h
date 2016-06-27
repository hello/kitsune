#ifndef __WIFI_CMD_H__
#define __WIFI_CMD_H__

#include "endpoints.h"
#include "periodic.pb.h"
#include "morpheus_ble.pb.h"
#include "kit_assert.h"


#define PILL_MAGIC 0xAAAAAAAA
#include "FreeRTOS.h"
#include "semphr.h"
#include "wlan.h"
#include "ble_cmd.h"
#include "network_types.h"

#ifndef MAX_SSID_LEN
#define MAX_SSID_LEN	(33)
#endif
#ifndef BSSID_LEN
#define BSSID_LEN	(8)
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

#define SERVER_REPLY_BUFSZ 2048
#define MAX_SHA256_SEND_SIZE 1024

#include "stdint.h"
#include "sync_response.pb.h"

#ifdef SELF_PROVISION_SUPPORT
#include "provision.pb.h"
#endif

#define INV_TIME 0xffffffff
extern
SyncResponse_Alarm alarm;

typedef struct {
	void (*get_reply_pb)(pb_field_t ** fields, void ** structdata);
	void (*on_pb_success)(void * structdata);
	void (*on_pb_failure)();
	void (*free_reply_pb)(void * structdata);
} protobuf_reply_callbacks;

typedef enum {
	SOCKET_SEC_NONE,
	SOCKET_SEC_SSL,
} security_type;

int Cmd_iperf_client(int argc, char *argv[]);
int Cmd_iperf_server(int argc, char *argv[]);
int Cmd_country(int argc, char *argv[]);


#define USER_DIR "/usr"
#define ANTENNA_FILE ("/hello/antenna")  // usr is in sd card, let's keep the original one.
#define ACCOUNT_ID_FILE "/hello/acct"
#define PROVISION_FILE "/pch/prov"
#define SERIAL_FILE "/pch/serial"
#define SERVER_SELECTION_FILE "/hello/dev"

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
int Cmd_setDns(int argc, char *argv[]);
void set_backup_dns();

int Cmd_RadioStartRX(int argc, char*argv[]);
int Cmd_RadioStopRX(int argc, char*argv[]);
int Cmd_RadioStopTX(int argc, char*argv[]);
int Cmd_RadioStartTX(int argc, char*argv[]);
int Cmd_RadioGetStats(int argc, char*argv[]);
int Cmd_setDev(int argc, char *argv[]);

bool get_mac(unsigned char mac[6]);
bool get_device_id(char * device_id, uint32_t size_of_device_id_buffer);

int match(char *regexp, char *text);
void load_aes();

void wifi_status_init();
int wifi_status_set(unsigned int status, int remove_status);
int wifi_status_get(unsigned int status);

bool send_periodic_data(batched_periodic_data* data, bool forced, int32_t to);
bool send_pill_data_generic(batched_pill_data * pill_data, const char * endpoint);

#ifdef SELF_PROVISION_SUPPORT
bool send_provision_request(ProvisionRequest* req);
#endif

#define DEFAULT_KEY "1234567891234567"

void thread_ota( void * unused );
int save_aes_in_memory(const uint8_t * key );
int get_aes(uint8_t * dst);
void on_key(uint8_t * key);
bool has_default_key();
bool should_burn_top_key();
void load_data_server();
int Cmd_burn_top(int argc, char *argv[]);

int send_data_pb( char* host, const char* path, char ** recv_buf_ptr,
		uint32_t * recv_buf_size_ptr, const pb_field_t fields[],  void * structdata,
		protobuf_reply_callbacks * pb_cb, int * sock, security_type sec );

int get_wifi_scan_result(Sl_WlanNetworkEntry_t* entries, uint16_t entry_len, uint32_t scan_duration_ms, int antenna);
SlSecParams_t make_sec_params(const char* ssid, const char* password, int sec_type, int version);
int connect_wifi(const char* ssid, const char* password, int sec_type, int version, int * idx, bool save);
void wifi_get_connected_ssid(uint8_t* ssid_buffer, size_t len);

long nwp_reset();
void wifi_reset();
void free_pill_list();
void reset_default_antenna();


#define MORPH_NAME ""

void telnetServerTask(void *);
void httpServerTask(void *);

#endif
