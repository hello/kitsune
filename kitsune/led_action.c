#include "led_action.h"
#include "led_cmd.h"
#include "led_animations.h"
#include "task.h"

 void led_action(const SyncResponse* response_protobuf)
{
    uint8_t alpha,a,r,g,b;
    int fade_in;
    int fade_out;
    unsigned int ud;
    int rot,idx;
    alpha = 0xFF;
    rot = 0;
    ud = 18;
    if (!response_protobuf->led_action.has_type || !response_protobuf->led_action.has_duration_seconds)
        return;

    fade_in = response_protobuf->led_action.duration_seconds/2;
    fade_out = response_protobuf->led_action.duration_seconds/2;
    switch(response_protobuf->led_action.type)
    {
    case SyncResponse_LEDAction_LEDActionType_FADEIO:
        break;
    case SyncResponse_LEDAction_LEDActionType_GLOW:
        ud = 64;
        break;
    case SyncResponse_LEDAction_LEDActionType_THROB:
        ud = 64;
        break;
    case SyncResponse_LEDAction_LEDActionType_PULSAR:
        rot = 1;
        break;
    case SyncResponse_LEDAction_LEDActionType_DOUBLE:
        rot = 1;
        break;
    case SyncResponse_LEDAction_LEDActionType_SIREN:
        rot = 1;
        break;
    case SyncResponse_LEDAction_LEDActionType_TRIPPY:
    	// TODO Test implementation, adopt non-blocking async.
        play_led_trippy(portMAX_DELAY);
        vTaskDelay((response_protobuf->led_action.duration_seconds * 1000)/portTICK_PERIOD_MS);
        stop_led_animation();

        return;
    case SyncResponse_LEDAction_LEDActionType_URL:
        rot = 1;
        break;
    }
    if (!response_protobuf->led_action.has_color)
        return;

    alpha = (uint8_t)((response_protobuf->led_action.color >> 24) & 0xFF);
    r = (uint8_t)((response_protobuf->led_action.color >> 16) & 0xFF);
    g = (uint8_t)((response_protobuf->led_action.color >> 8) & 0xFF);
    b = (uint8_t)(response_protobuf->led_action.color & 0xFF);

    led_set_color(alpha, r, g, b, fade_in, fade_out, ud, rot);
}

int Cmd_led_action(int argc, char *argv[]) {
    SyncResponse pb;
    if (argc != 4)
        return -1;

    memset(&pb,0,sizeof(pb));
    pb.led_action.has_type = 1;
    pb.led_action.type = (SyncResponse_LEDAction_LEDActionType)atoi(argv[1]);
    pb.led_action.has_color = 1;
    pb.led_action.color = (uint32_t)atoi(argv[2]);
    pb.led_action.has_duration_seconds = 1;
    pb.led_action.duration_seconds = atoi(argv[3]);

    led_action((const SyncResponse*)&pb);
    return 0;
}

//end led_action.c
