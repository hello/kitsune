#ifndef __LED_ACTION_H__
#define __LED_ACTION_H__

#include "wifi_cmd.h"

int Cmd_led_action(int argc, char *argv[]);
void led_action(const SyncResponse* response_protobuf);

#endif // __LED_ACTION_H__
