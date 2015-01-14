#include "led_action.h"
#include "led_cmd.h"
#include "led_animations.h"
#include "task.h"
#include "uartstdio.h"
#include "utils.h"

#define MAX_ACTION_WAIT_TICKS 2000
#define FLAG_FADE	(1 << 0)
#define FLAG_ROTATE	(1 << 1)
#define FAST_FADE_FACTOR 18
#define SLOW_FADE_FACTOR 6

typedef struct color_t{
	uint8_t r,g,b;
}color_t;

typedef struct pattern_t{
	uint8_t map[NUM_LED]; // 12 LEDs * 1 byte per led (color map entry)
}pattern_t;

typedef struct animation_t{
	int type; 						// Type of animation, 0 - default, > 0 future animation types
	int idx; 						// animation pattern index
	int num_colors;					// Max 256
	color_t *p_colors;				// Color map array
	int num_patterns;				// Max 256
	pattern_t *p_patterns;			// Pattern array
}animation_t;

#ifdef TEST_PATTERN_ANIMATION
static pattern_t s_test_pattern[] =
{
	{ 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 3, 2 },
	{ 2, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 3 },
	{ 3, 2, 1, 2, 3, 0, 0, 0, 0, 0, 0, 0 },
	{ 0, 3, 2, 1, 2, 3, 0, 0, 0, 0, 0, 0 },
	{ 0, 0, 3, 2, 1, 2, 3, 0, 0, 0, 0, 0 },
	{ 0, 0, 0, 3, 2, 1, 2, 3, 0, 0, 0, 0 },
	{ 0, 0, 0, 0, 3, 2, 1, 2, 3, 0, 0, 0 },
	{ 0, 0, 0, 0, 0, 3, 2, 1, 2, 3, 0, 0 },
	{ 0, 0, 0, 0, 0, 0, 3, 2, 1, 2, 3, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 3, 2, 1, 2, 3 },
	{ 3, 0, 0, 0, 0, 0, 0, 0, 3, 2, 1, 2 },
	{ 2, 3, 0, 0, 0, 0, 0, 0, 0, 3, 2, 1 },
};
static int s_test_pattern_num = sizeof(s_test_pattern)/sizeof(s_test_pattern[0]);
static color_t s_test_map[] =
{
		{ 0x00, 0x00, 0x00 },
		{ 0xFE, 0x00, 0x00 },
		{ 0x8E, 0x00, 0x00 },
		{ 0x3E, 0x00, 0x00 },
};
static int s_test_map_num = sizeof(s_test_map)/sizeof(s_test_map[0]);
#endif

typedef struct siren_t{
	int idx;
	uint32_t mask;
	uint32_t alt_mask;
}siren_t;

typedef struct trippy_t{
	int idx;
	color_t colors[NUM_LED];
	color_t prev_colors[NUM_LED];
}trippy_t;
typedef struct led_action_t{
	bool init;
	xSemaphoreHandle sem;
	TickType_t end_ticks;
	SyncResponse_LEDAction_LEDActionType type;
	color_t color;
	color_t alt_color;
	uint32_t delay;								// delay between led cycles in ms (see led_cmd)
	uint32_t flags;
	TickType_t end_fadein_ticks;
	TickType_t start_fadeout_ticks;
	int brightness;
	int factor;
	union
	{
		siren_t siren;
		animation_t anim;
		trippy_t trippy;
	};
}led_action_t;

static led_action_t s_la = { false };

void led_action_init(led_action_t *p_la)
{
	memset(p_la, 0 ,sizeof(led_action_t));
	vSemaphoreCreateBinary(p_la->sem);
	p_la->init = true;

}
static bool led_action_default(led_action_t *p_la,
							int * out_r, int * out_g, int * out_b,
							int * out_delay, void * user_context, int rgb_array_size){
	int i = 0;
	for(i = 0; i < NUM_LED; i++) {
		out_r[i] =  p_la->color.r;
		out_g[i] =  p_la->color.g;
		out_b[i] =  p_la->color.b;
	}
	*out_delay = p_la->delay;

	return true;
}
static bool led_action_animation(led_action_t *p_la,
							int * out_r, int * out_g, int * out_b,
							int * out_delay, void * user_context, int rgb_array_size){
	int i = 0;
	for(i = 0; i < NUM_LED; i++) {
		uint8_t *map = p_la->anim.p_patterns[p_la->anim.idx].map;
		color_t *color = &(p_la->anim.p_colors[map[i]]);
		out_r[i] =  color->r;
		out_g[i] =  color->g;
		out_b[i] =  color->b;
	}
	*out_delay = p_la->delay;
	p_la->anim.idx++;
	if (p_la->anim.idx >= NUM_LED)
		p_la->anim.idx = 0;

	return true;
}

static bool led_action_siren(led_action_t *p_la,
							int * out_r, int * out_g, int * out_b,
							int * out_delay, void * user_context, int rgb_array_size){
	int i = 0;
	uint32_t mask, alt_mask;

	mask = (p_la->siren.mask >> p_la->siren.idx);
	alt_mask = (p_la->siren.alt_mask >> p_la->siren.idx);
	for(i = 0; i < NUM_LED; i++) {
		if ((1 << i) & mask)
		{
			out_r[i] =  p_la->color.r;
			out_g[i] =  p_la->color.g;
			out_b[i] =  p_la->color.b;
		}
		else if ((1 << i) & alt_mask)
		{
			out_r[i] =  p_la->alt_color.r;
			out_g[i] =  p_la->alt_color.g;
			out_b[i] =  p_la->alt_color.b;
		}
	}
	*out_delay = p_la->delay;
	p_la->siren.idx++;
	if (p_la->siren.idx >= NUM_LED)
		p_la->siren.idx = 0;

	return true;
}
static bool led_action_party(led_action_t *p_la,
							int * out_r, int * out_g, int * out_b,
							int * out_delay, void * user_context, int rgb_array_size){
	int i = 0;

	for(i = 0; i < NUM_LED; i++){
		out_r[i] =  rand() % LED_MAX;
		out_g[i] =  rand() % LED_MAX;
		out_b[i] =  rand() % LED_MAX;
	}
	*out_delay = p_la->delay;

	return true;
}

// TODO refactor so there are not two trippy animations

static bool _reach_color(uint8_t * v, uint8_t target){
	if(*v == target){
		return true;
	}else if(*v < target){
		*v = *v + 1;
	}else{
		*v = *v - 1;
	}
	return false;
}

static bool led_action_trippy(led_action_t *p_la,
							int * out_r, int * out_g, int * out_b,
							int * out_delay, void * user_context, int rgb_array_size){
	int i = 0;
	for(i = 0; i < NUM_LED; i++){
		if(_reach_color(&p_la->trippy.prev_colors[i].r, p_la->trippy.colors[i].r)){
			p_la->trippy.colors[i].r = rand()%32 + (p_la->trippy.idx++)%32;
		}
		if(_reach_color(&p_la->trippy.prev_colors[i].g, p_la->trippy.colors[i].g)){
			p_la->trippy.colors[i].g = rand()%32 + (p_la->trippy.idx++)%32;
		}
		if(_reach_color(&p_la->trippy.prev_colors[i].b, p_la->trippy.colors[i].b)){
			p_la->trippy.colors[i].b = rand()%32 + (p_la->trippy.idx++)%32;
		}
		out_r[i] = p_la->trippy.prev_colors[i].r;
		out_g[i] = p_la->trippy.prev_colors[i].g;
		out_b[i] = p_la->trippy.prev_colors[i].b;
	}
	*out_delay = p_la->delay;

	return true;
}

static void led_action_handle_fade(led_action_t *p_la,
									int * out_r, int * out_g, int * out_b ) {
	int i;
	TickType_t ticks = xTaskGetTickCount();
	if (!(p_la->flags & FLAG_FADE))
		return;

	if ((ticks > p_la->end_fadein_ticks) && (ticks < p_la->start_fadeout_ticks))
		return;

	if (ticks > p_la->start_fadeout_ticks)
		p_la->brightness -= p_la->factor;
	else if (ticks < p_la->end_fadein_ticks)
		p_la->brightness += p_la->factor;

	if (p_la->brightness > LED_MAX)
		p_la->brightness = LED_MAX;
	else if (p_la->brightness < 0)
		p_la->brightness = 0;

	// LOGE("brightness = %d \r\n",p_la->brightness);

	for (i = 0; i < NUM_LED; ++i) {
		out_r[i] = ((p_la->brightness * out_r[i]) >> 8) & 0xFF;
		out_g[i] = ((p_la->brightness * out_g[i]) >> 8) & 0xFF;
		out_b[i] = ((p_la->brightness * out_b[i]) >> 8) & 0xFF;
	}
	//LOGE("color = %d,%d,%d\r\n",out_r[0],out_g[0],out_b[0]);
}

static bool led_action_cb(int * out_r, int * out_g, int * out_b,
						int * out_delay, void * user_context, int rgb_array_size){
	bool result = false;
	led_action_t *p_la = (led_action_t *)user_context;
	TickType_t ticks = xTaskGetTickCount();
	int ticks_per;

	//LOGE("type %d, rgb_array_size = %d\r\n",p_la->type,rgb_array_size);
	if (rgb_array_size > NUM_LED)
	{
		LOGE("rgb_array_size > NUM_LED\r\n");
    	xSemaphoreGive(p_la->sem);
		return false;
	}

    switch(p_la->type)
    {
    case SyncResponse_LEDAction_LEDActionType_FADEIO:	// Quickly fade in color, wait, then quickly fade out
    	result = led_action_default(p_la, out_r,out_g,out_b,out_delay,user_context,rgb_array_size);
        break;
    case SyncResponse_LEDAction_LEDActionType_GLOW:		// Slowly fade in color for half duration, then slowly fade out
    	result = led_action_default(p_la, out_r,out_g,out_b,out_delay,user_context,rgb_array_size);
        break;
    case SyncResponse_LEDAction_LEDActionType_THROB:	// repeadedly fade in to color and out again
    	ticks_per = LED_MAX/p_la->factor;  // number of cycles in a fade
    	ticks_per *= p_la->delay;      // number of ms in a fade
    	ticks_per = ticks_per / portTICK_PERIOD_MS; // number of ticks in a fade

    	if (ticks > (p_la->start_fadeout_ticks + ticks_per))
    	{
    		p_la->end_fadein_ticks = p_la->start_fadeout_ticks + (ticks_per * 2);
    		p_la->start_fadeout_ticks = p_la->end_fadein_ticks;
    		if (p_la->start_fadeout_ticks  + ticks_per > p_la->end_ticks)
    		{
    			p_la->start_fadeout_ticks = p_la->end_ticks - ticks_per;
    			p_la->end_fadein_ticks = p_la->start_fadeout_ticks;
    		}
    	}
    	result = led_action_default(p_la, out_r,out_g,out_b,out_delay,user_context,rgb_array_size);
        break;
    case SyncResponse_LEDAction_LEDActionType_PULSAR:	// Spining pulsar effect with quick fade in color, wait then quick fade out
    case SyncResponse_LEDAction_LEDActionType_DOUBLE:	// Double spining pulsar effect with quick fade in color, wait then quick fade out
    case SyncResponse_LEDAction_LEDActionType_SIREN:	// Double spining pulsar with both colors with quick fade in, wait, then quick fade out
    	result = led_action_siren(p_la, out_r,out_g,out_b,out_delay,user_context,rgb_array_size);
        break;
    case SyncResponse_LEDAction_LEDActionType_TRIPPY:	// Randomized LED colors
    	result = led_action_trippy(p_la, out_r,out_g,out_b,out_delay,user_context,rgb_array_size);
        break ;
    case SyncResponse_LEDAction_LEDActionType_PARTY:	// Party mode, fade to/from random colors
    	result = led_action_party(p_la, out_r,out_g,out_b,out_delay,user_context,rgb_array_size);
        break ;
    case SyncResponse_LEDAction_LEDActionType_URL:		// URL to a multi color LED blink pattern definition
    	result = led_action_animation(p_la, out_r,out_g,out_b,out_delay,user_context,rgb_array_size);
        break;
    }

    led_action_handle_fade(p_la, out_r,out_g,out_b);

    if (!result || (p_la->end_ticks < xTaskGetTickCount()))
    {
    	xSemaphoreGive(p_la->sem);
    	LOGE("%s %d DONE!\r\n", __FUNCTION__,result);
    	result = false;
    }

    return result;
}

bool led_action_start(led_action_t *p_la,const SyncResponse* p_rb ) {
	int i, dur_ticks;
	TickType_t ticks = xTaskGetTickCount();

	// LOGE("%s\r\n", __FUNCTION__);
	if(xSemaphoreTake(p_la->sem, MAX_ACTION_WAIT_TICKS) == pdPASS)
	{
		p_la->delay = 32;
		p_la->flags = FLAG_FADE;
		p_la->factor = FAST_FADE_FACTOR;
		p_la->brightness = 0;
		p_la->type = p_rb->led_action.type;
		p_la->factor = SLOW_FADE_FACTOR;
		dur_ticks =  ((p_rb->led_action.duration_seconds * 1000)/portTICK_PERIOD_MS);
		p_la->end_ticks = ticks + dur_ticks;

		p_la->color.r = (uint8_t)((p_rb->led_action.color >> 16) & 0xFF);
		p_la->color.g = (uint8_t)((p_rb->led_action.color >> 8) & 0xFF);
		p_la->color.b = (uint8_t)(p_rb->led_action.color & 0xFF);
		if (p_rb->led_action.has_alt_color)
		{
			p_la->alt_color.r = (uint8_t)((p_rb->led_action.alt_color >> 16) & 0xFF);
			p_la->alt_color.g = (uint8_t)((p_rb->led_action.alt_color >> 8) & 0xFF);
			p_la->alt_color.b = (uint8_t)(p_rb->led_action.alt_color & 0xFF);
		}

	    switch(p_la->type)
	    {
	    case SyncResponse_LEDAction_LEDActionType_FADEIO:
	        break;
	    case SyncResponse_LEDAction_LEDActionType_GLOW:
			p_la->factor = SLOW_FADE_FACTOR;
	        break;
	    case SyncResponse_LEDAction_LEDActionType_THROB:
			p_la->factor = SLOW_FADE_FACTOR;
	        break;
	    case SyncResponse_LEDAction_LEDActionType_PULSAR:
			p_la->alt_color.r = 0;
			p_la->alt_color.g = 0;
			p_la->alt_color.b = 0;
	        // fall through
			//break
	    case SyncResponse_LEDAction_LEDActionType_DOUBLE:
	    	p_la->siren.idx = 0;
	    	p_la->siren.mask = 0x40040;
	    	p_la->siren.alt_mask = 0x01001;
			p_la->delay = 64;
			break;
	    case SyncResponse_LEDAction_LEDActionType_SIREN:
	    	p_la->siren.idx = 0;
	    	p_la->siren.mask = 0x3F03F;
	    	p_la->siren.alt_mask = 0xFC0FC0;
			p_la->delay = 64;
	        break;
	    case SyncResponse_LEDAction_LEDActionType_PARTY:
	        break;
	    case SyncResponse_LEDAction_LEDActionType_TRIPPY:
	    	p_la->trippy.idx = 0;
			for(i = 0; i < NUM_LED; i++){
				p_la->trippy.colors[i] = (color_t){rand()%120, rand()%120, rand()%120};
				p_la->trippy.prev_colors[i] = (color_t){0};
			}
	        break ;
	    case SyncResponse_LEDAction_LEDActionType_URL:
	    	p_la->anim.idx = 0;
#ifdef TEST_PATTERN_ANIMATION
	    	// TODO get pattern animation from URL, for now use test animation
	    	p_la->anim.num_colors = s_test_map_num;
	    	p_la->anim.p_colors = s_test_map;
	    	p_la->anim.num_patterns = s_test_pattern_num;
	    	p_la->anim.p_patterns = s_test_pattern;
#endif
	        break;
	    }
	    i = LED_MAX/p_la->factor;  // number of cycles in a fade
	    i *= p_la->delay;      // number of ms in a fade
	    i = i / portTICK_PERIOD_MS; // number of ticks in a fade
		// LOGE("ticks in fade = %d\r\n", i);
		// LOGE("dur_ticks = %d\r\n", dur_ticks);

		if (i > dur_ticks/2)
			p_la->flags &= ~FLAG_FADE; // don't fade if duration isn't long enough
		else {
			p_la->end_fadein_ticks = ticks + i;
			p_la->start_fadeout_ticks = p_la->end_ticks - i;
			if (p_la->type == SyncResponse_LEDAction_LEDActionType_THROB)
				p_la->start_fadeout_ticks = p_la->end_fadein_ticks;
		}

	    // LOGE("%s led_start_custom_animation\r\n", __FUNCTION__);
		led_start_custom_animation(led_action_cb, p_la);
		return true;
	}
	return false;
}


 void led_action(const SyncResponse* response_protobuf)
{
	 // LOGE("%s\r\n", __FUNCTION__);
	 if (!s_la.init)
		 led_action_init(&s_la);
	 led_action_start(&s_la,response_protobuf);
}

int Cmd_led_action(int argc, char *argv[]) {
    SyncResponse pb;
    if (argc != 4)
        return -1;

    memset(&pb,0,sizeof(pb));
    pb.led_action.has_type = 1;
    pb.led_action.type = (SyncResponse_LEDAction_LEDActionType)atoi(argv[1]);
    pb.led_action.has_color = 1;
    pb.led_action.color = (uint32_t)0xff00ff00;
    pb.led_action.has_alt_color = 1;
    pb.led_action.alt_color = (uint32_t)0xffff0000;
    pb.led_action.has_duration_seconds = 1;
    pb.led_action.duration_seconds = atoi(argv[3]);

    led_action((const SyncResponse*)&pb);
    return 0;
}
//end led_action.c
