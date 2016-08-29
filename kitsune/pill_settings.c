#include "simplelink.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "sl_sync_include_after_simplelink_header.h"

#include "uartstdio.h"
#include "ustdlib.h"
#include "pill_settings.h"
#include "kit_assert.h"

static SyncResponse_PillSettings * _settings;
static int _num_settings = 0;
static xSemaphoreHandle _sync_mutex;

bool on_pill_settings(pb_istream_t *stream, const pb_field_t *field, void **arg) {
	SyncResponse_PillSettings pill_setting;

	if( !pb_decode(stream,SyncResponse_PillSettings_fields,&pill_setting) ) {
		LOGI("pill settings parse fail \n" );
		return false;
	}
	if( !( pill_setting.has_pill_id && pill_setting.has_pill_color ) ) {
		LOGI("pill settings incomplete \n" );
		return false;
	}

    int i;
    xSemaphoreTake(_sync_mutex, portMAX_DELAY);
    for(i = 0; i < _num_settings; i++)
    {
        if(strcmp(_settings[i].pill_id, pill_setting.pill_id) == 0 )
        {
        	if( _settings[i].pill_color != pill_setting.pill_color ) {
        		LOGI("Updated pill %s to %x\n", pill_setting.pill_id, pill_setting.pill_color);
            	_settings[i] = pill_setting;
        	}
            xSemaphoreGive(_sync_mutex);
        	return true;
        }
    }

	LOGI("New pill %s in %x\n", pill_setting.pill_id, pill_setting.pill_color);
	_settings = pvPortRealloc(_settings, (_num_settings+1)*sizeof(SyncResponse_PillSettings));
	_settings[_num_settings] = pill_setting;
	_num_settings += 1;
    xSemaphoreGive(_sync_mutex);

    return true;

}

int pill_settings_pill_count()
{
	return _num_settings;
}


uint32_t pill_settings_get_color(const char* pill_id)
{
    int i = 0;
    if(xSemaphoreTake(_sync_mutex, portMAX_DELAY))
    {
        for(i = 0; i < _num_settings; i++)
        {
            if(strcmp(_settings[i].pill_id, pill_id) == 0)
            {
            	uint32_t color = _settings[i].pill_color;
            	LOGI("%s color found %d\n", _settings[i].pill_id, _settings[i].pill_color);
            	xSemaphoreGive(_sync_mutex);
                return color;
            }
        }
        LOGI("%s color not found\n", _settings[i].pill_id);
        xSemaphoreGive(_sync_mutex);
    }

    return 0;  // the default purple color
}

void pill_settings_init()
{
    _sync_mutex = xSemaphoreCreateMutex();
    xSemaphoreTake(_sync_mutex, portMAX_DELAY);
    _num_settings = 0;
    int i = 0;
    for(i = 0; i < _num_settings; i++)
    {
    	LOGI("%s color loaded %d\n", _settings[i].pill_id, _settings[i].pill_color);
    }
    xSemaphoreGive(_sync_mutex);
}

void pill_settings_reset_all()
{
    xSemaphoreTake(_sync_mutex, portMAX_DELAY);
    _num_settings = 0;
    vPortFree(_settings);
    _settings = NULL;
    xSemaphoreGive(_sync_mutex);

}
