#ifndef LED_CMD_H
#define LED_CMD_H

#ifdef __cplusplus
extern "C" {
#endif

int Cmd_led_clr(int argc, char *argv[]);
int Cmd_led(int argc, char *argv[]);
int led_init(void);
void led_task( void * params );

//helper api
int led_set_color(int r, int g, int b, int fade);
#ifdef __cplusplus
}
#endif
#endif
