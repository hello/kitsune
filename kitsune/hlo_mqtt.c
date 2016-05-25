#include "hlo_mqtt.h"
#include "hlo_http.h"
#include "uart_logger.h"

extern hlo_stream_t * hlo_sock_stream_opt(unsigned long ip, uint16_t port, uint8_t secure);
extern unsigned long hlo_http_get_ip(const char * host);

#define MQTT_HOST "test.mosquitto.org"
static hlo_stream_t * sock;
static void * mqtt_ctx;

static void _on_client_ack_notify(void *app, u8 msg_type, u16 msg_id, u8 *buf, u32 len){

}
static bool _on_client_message(void *app,
                           bool dup, enum mqtt_qos qos, bool retain,
                           struct mqtt_packet *mqp){

}
static void _on_client_disconnect(void *app, i32 cause){

}
static i32  _client_printf(const c8 *format, ...){
	va_list vaArgP;
    va_start(vaArgP, format);
    uart_logf(LOG_INFO, format,vaArgP);
    va_end(vaArgP);
    return 0;
}

static void _lock_mutex(void *mutex){
	xSemaphoreTakeRecursive(mutex, portMAX_DELAY);
}
static void _unlock_mutex(void *mutex){
	xSemaphoreGiveRecursive(mutex);
}
static i32 _open_sock(u32 nwconn_opts, const c8 *server_addr, u16 port_number, const struct secure_conn *nw_security){
	return 0;
}
static i32 _send_sock(i32 comm, const u8 *buf, u32 len, void *ctx){
	return 0;
}
static i32 _recv_sock(i32 comm, u8 *buf, u32 len, u32 wait_secs, bool *err_timeo, void *ctx){
	return 0;
}
static i32 _send_dst(i32 comm, const u8 *buf, u32 len, u16 dest_port, const u8 *dest_ip, u32 ip_len){
	return 0;
}
static i32 _recv_from(i32 comm, u8 *buf, u32 len, u16 *from_port, u8 *from_ip, u32 *ip_len){
	return 0;
}
static i32 _close_sock(i32 comm){
	return 0;
}
static i32 _listen_sock(u32 nwconn_opts, u16 port_number, const struct secure_conn *nw_security){
	return 0;
}
static i32 _accept_sock(i32 listen, u8 *client_ip, u32 *ip_length){
	return 0;
}
static i32 _io_mon(i32 *recv_cvec, i32 *send_cvec, i32 *rsvd_cvec,  u32 wait_secs){
	return 0;
}
static u32 _get_time(void){
	return 0;
}
int hlo_mqtt_init(void){
	struct mqtt_client_ctx_cfg cfg;
	struct mqtt_client_ctx_cbs cbs;
	struct mqtt_client_lib_cfg lib_cfg;
	struct device_net_services net;

	lib_cfg.aux_debug_en = 1;
	lib_cfg.debug_printf = _client_printf;
	lib_cfg.grp_uses_cbfn = 1;
	lib_cfg.loopback_port = 0;
	lib_cfg.mutex = xSemaphoreCreateRecursiveMutex();
	lib_cfg.mutex_lockin = _lock_mutex;
	lib_cfg.mutex_unlock = _unlock_mutex;
	if( 0 != mqtt_client_lib_init(&lib_cfg) ){
		goto fail_mqtt_init;
	}

	sock = hlo_sock_stream_opt(hlo_http_get_ip(MQTT_HOST),1883, 0);
	if( !sock ) {
		goto fail_sock;
	}
	cfg.config_opts = MQTT_CFG_APP_HAS_RTSK;
	cfg.nw_security = NULL; /* we handle our own security */
	cfg.nwconn_opts = 0;
	cfg.port_number = 1883;
	cfg.server_addr = MQTT_HOST;

	cbs.ack_notify = _on_client_ack_notify;
	cbs.disconn_cb = _on_client_disconnect;
	cbs.publish_rx = _on_client_message;

	if(0 != mqtt_client_ctx_create(&cfg, &cbs, NULL, &mqtt_ctx)){
		goto fail_ctx;
	}

	net.accept = _accept_sock;
	net.close = _close_sock;
	net.io_mon = _io_mon;
	net.listen = _listen_sock;
	net.open = _open_sock;
	net.recv = _recv_sock;
	net.recv_from = _recv_from;
	net.send = _send_sock;
	net.send_dest = _send_dst;
	net.time = _get_time;
	if( 0 !=  mqtt_client_net_svc_register(&net)){
		goto fail_svc;
	}
	return 0;
fail_svc:
	mqtt_client_ctx_delete(mqtt_ctx);
fail_ctx:
	hlo_stream_close(sock);
fail_sock:
fail_mqtt_init:
	return -1;
}

int hlo_mqtt_publish(const char * topic, const char * buf, size_t buf_len){
	return 0;
}
