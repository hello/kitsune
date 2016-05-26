#include "hlo_mqtt.h"
#include "hlo_http.h"
#include "uart_logger.h"
#include "hlo_async.h"
#include <stdarg.h>

extern hlo_stream_t * hlo_sock_stream_opt(unsigned long ip, uint16_t port, uint8_t secure);
extern unsigned long hlo_http_get_ip(const char * host);

#define MQTT_HOST "test.mosquitto.org"
#define MQTT_BUFFER_COUNT 4
#define MQTT_BUFFER_SIZE 32
static hlo_stream_t * sock;
static void * mqtt_ctx;

static void _on_client_ack_notify(void *app, u8 msg_type, u16 msg_id, u8 *buf, u32 len){

}
static bool _on_client_message(void *app,
                           bool dup, enum mqtt_qos qos, bool retain,
                           struct mqtt_packet *mqp){
	uint8_t buf[16] = {0};
	memcpy(buf, MQP_PUB_PAY_BUF(mqp), MQP_PUB_PAY_LEN(mqp));
	DISP("=========================\r\nCan Has message %s\r\n", buf);

	return true;
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
	DISP("MQTT: Open\r\n");
	sock = hlo_sock_stream_opt(hlo_http_get_ip(MQTT_HOST),1883, 0);
	if(sock){
		return 1;
	}else{
		return 0;
	}

}
static i32 _send_sock(i32 comm, const u8 *buf, u32 len, void *ctx){
	DISP("MQTT: Send\r\n");
	int ret = hlo_stream_write(sock,buf,len);
	return ret;

}
static i32 _recv_sock(i32 comm, u8 *buf, u32 len, u32 wait_secs, bool *err_timeo, void *ctx){
	DISP("MQTT: Recv TO %u\r\n", wait_secs);
	TickType_t end = xTaskGetTickCount() + wait_secs * 1000;
	int ret = 0;
	while(xTaskGetTickCount() < end && ret == 0){
		ret = hlo_stream_read(sock, buf, len);
	}
	if( ret == 0){
		*err_timeo = 1;
		return -1;
	}else if(ret == HLO_STREAM_EOF){
		return 0;
	}
	return ret;
}
static i32 _send_dst(i32 comm, const u8 *buf, u32 len, u16 dest_port, const u8 *dest_ip, u32 ip_len){
	DISP("MQTT: Send_DST\r\n");
	return 0;
}
static i32 _recv_from(i32 comm, u8 *buf, u32 len, u16 *from_port, u8 *from_ip, u32 *ip_len){
	DISP("MQTT: Recv_From\r\n");
	return 0;
}
static i32 _close_sock(i32 comm){
	DISP("MQTT: Close\r\n");
	hlo_stream_close(sock);
	return 0;
}
static i32 _listen_sock(u32 nwconn_opts, u16 port_number, const struct secure_conn *nw_security){
	DISP("MQTT: Listen\r\n");
	return 0;
}
static i32 _accept_sock(i32 listen, u8 *client_ip, u32 *ip_length){
	DISP("MQTT: Accept\r\n");
	return 0;
}
static i32 _io_mon(i32 *recv_cvec, i32 *send_cvec, i32 *rsvd_cvec,  u32 wait_secs){
	DISP("MQTT: IO_MON\r\n");
	vTaskDelay(wait_secs * 1000);
	return 0;
}
static u32 _get_time(void){
	DISP("MQTT: Time\r\n");
	return xTaskGetTickCount()/1000;
}
static void mqttd(hlo_future_t * f, void * ctx){
	while(1){
		mqtt_client_ctx_run(ctx, 10000);
		vTaskDelay(1000);
		DISP("MQTT Restart\r\n");
		if(!mqtt_client_is_connected(ctx)){
			LOGI("MQTT: Connecting\r\n");
			int ret = mqtt_connect_msg_send(ctx, 1, 10);
			LOGI("Done connect %d\r\n", ret);
		}
	}
}
int hlo_mqtt_init(void){
	static struct mqtt_client_ctx_cfg cfg;
	static struct mqtt_client_ctx_cbs cbs;
	static struct mqtt_client_lib_cfg lib_cfg;
	static struct device_net_services net;
	int ret;


	static struct mqtt_packet packets[MQTT_BUFFER_COUNT];
	static u8 mqtt_buf_vec[MQTT_BUFFER_COUNT * MQTT_BUFFER_SIZE];
	mqtt_client_buffers_register(MQTT_BUFFER_COUNT, packets, MQTT_BUFFER_SIZE, mqtt_buf_vec);


	lib_cfg.aux_debug_en = 1;
	lib_cfg.debug_printf = _client_printf;
	lib_cfg.grp_uses_cbfn = 1;
	lib_cfg.loopback_port = 0;
	lib_cfg.mutex = xSemaphoreCreateRecursiveMutex();
	lib_cfg.mutex_lockin = _lock_mutex;
	lib_cfg.mutex_unlock = _unlock_mutex;
	if( 0 != (ret = mqtt_client_lib_init(&lib_cfg)) ){
		DISP("f0 %d\r\n", ret);
		goto fail_mqtt_init;
	}

	cfg.config_opts = MQTT_CFG_APP_HAS_RTSK;
	cfg.nw_security = NULL; /* we handle our own security */
	cfg.nwconn_opts = 0;
	cfg.port_number = 1883;
	cfg.server_addr = MQTT_HOST;

	cbs.ack_notify = _on_client_ack_notify;
	cbs.disconn_cb = _on_client_disconnect;
	cbs.publish_rx = _on_client_message;

	if(0 != (ret = mqtt_client_ctx_create(&cfg, &cbs, NULL, &mqtt_ctx))){
		DISP("f1 %d\r\n", ret);
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
	if( 0 !=  (ret = mqtt_client_net_svc_register(&net))){
		DISP("f2 %d\r\n", ret);
		goto fail_svc;
	}
	DISP("mqtt ok %d\r\n", ret);
	hlo_future_create_task_bg(mqttd, mqtt_ctx, 2048);
	return 0;
fail_svc:
	mqtt_client_ctx_delete(mqtt_ctx);
fail_ctx:
fail_mqtt_init:
	return -1;
}

int hlo_mqtt_publish(const char * topic, const char * buf, size_t buf_len){
	return 0;
}
int Cmd_MqttConnect(int argc, char * argv[]){
	hlo_mqtt_init();

	while(!mqtt_client_is_connected(mqtt_ctx)){
		vTaskDelay(1000);
	}
	struct utf8_strqos req = (struct utf8_strqos){
			.buffer = "morpheus",
			.length = 8,
			.qosreq = MQTT_QOS1,
	};
	int ret = mqtt_sub_msg_send(mqtt_ctx, &req, 1);
	LOGI("Done sub %d\r\n", ret);
	return 0;
}
int Cmd_MqttPub(int argc, char * argv[]){
	struct utf8_string topic = (struct utf8_string){
		.buffer = "morpheus",
		.length = 8,
	};
	int ret = mqtt_client_pub_msg_send(mqtt_ctx, &topic, argv[1], strlen(argv[1]), MQTT_QOS1, 0);
	LOGI("Done pub %d\r\n", ret);
	return 0;
}
