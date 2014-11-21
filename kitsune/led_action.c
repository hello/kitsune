#include "led_action.h"
#include "led_cmd.h"
#include "led_animations.h"

typedef struct led_color_map_t
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
}led_color_map_t;

static const led_color_map_t s_led_color_map[] =
{
    {LED_MAX,   0,          0       },  // SyncResponse_LEDAction_LEDActionColor_RED = 0
    {0,         LED_MAX,    0       },  // SyncResponse_LEDAction_LEDActionColor_GREEN = 1
    {0,         0,          LED_MAX },  // SyncResponse_LEDAction_LEDActionColor_BLUE = 2,
    {0,         0,          50      },  // SyncResponse_LEDAction_LEDActionColor_LT_BLUE = 3,
    {LED_MAX,   LED_MAX,    0       },  // SyncResponse_LEDAction_LEDActionColor_YELLOW = 4,
    {LED_MAX,   LED_MAX,    0       },  // SyncResponse_LEDAction_LEDActionColor_WARN = 5,
    {0xF0,      0x76,       0       },  // SyncResponse_LEDAction_LEDActionColor_ALERT = 6,
    {LED_MAX,   LED_MAX,    LED_MAX },  // SyncResponse_LEDAction_LEDActionColor_WHITE = 7
};

 void led_action(const SyncResponse* response_protobuf)
{
    uint8_t alpha;
    int fade_in;
    int fade_out;
    unsigned int ud;
    int rot,idx;
    alpha = 0xFF;
    rot = 0;
    ud = 18;
    if (!response_protobuf->led_action.has_type || !response_protobuf->led_action.has_duration)
        return;

    fade_in = response_protobuf->led_action.duration/2;
    fade_out = response_protobuf->led_action.duration/2;
    switch(response_protobuf->led_action.type)
    {
    case SyncResponse_LEDAction_LEDActionType_FLASH:
        break;
    case SyncResponse_LEDAction_LEDActionType_GLOW:
        ud = 64;
        break;
    case SyncResponse_LEDAction_LEDActionType_PULSAR:
        rot = 1;
        break;
    case SyncResponse_LEDAction_LEDActionType_TRIPPY:
    	// TODO Test implementation, adopt non-blocking async.
        play_led_trippy(portMAX_DELAY);
        //vTaskDelay((response_protobuf->led_action.duration * 1000)/portTICK_PERIOD_MS);
        vTaskDelay(2000);
        stop_led_animation();

        return;
    }
    if (!response_protobuf->led_action.has_color)
        return;

    idx = response_protobuf->led_action.color;

    led_set_color(alpha, s_led_color_map[idx].r, s_led_color_map[idx].g, s_led_color_map[idx].b, fade_in, fade_out, ud, rot);
}

int Cmd_led_action(int argc, char *argv[]) {
    SyncResponse pb;
    if (argc != 4)
        return -1;

    pb.led_action.has_type = 1;
    pb.led_action.type = (SyncResponse_LEDAction_LEDActionType)atoi(argv[1]);
    pb.led_action.has_color = 1;
    pb.led_action.color = (SyncResponse_LEDAction_LEDActionColor)atoi(argv[2]);
    pb.led_action.has_duration = 1;
    pb.led_action.duration = atoi(argv[3]);

    led_action((const SyncResponse*)&pb);
    return 0;
}

//end led_action.c
