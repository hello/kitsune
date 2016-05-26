#include "mqtt/mqtt_client.h"

int hlo_mqtt_init(void);

int hlo_mqtt_publish(const char * topic, const char * buf, size_t buf_len);

int Cmd_MqttConnect(int argc, char * argv[]);
int Cmd_MqttPub(int argc, char * argv[]);
