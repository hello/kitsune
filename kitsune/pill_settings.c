#include "simplelink.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "sl_sync_include_after_simplelink_header.h"

#include "uartstdio.h"
#include "ustdlib.h"
#include "pill_settings.h"

#define MAX_BUFF_SIZE   128  // This is barely enough

static BatchedPillSettings _settings;
static xSemaphoreHandle _sync_mutex;

#include "fs_utils.h"

int pill_settings_save(BatchedPillSettings* pill_settings)
{
    if(!pill_settings)
    {
        return 0;
    }

    int has_change = 0;
    int i;
    xSemaphoreTake(_sync_mutex, portMAX_DELAY);
    for(i = 0; i < MAX_PILL_SETTINGS_COUNT; i++)
    {
        if(strcmp(_settings.pill_settings[i].pill_id, pill_settings->pill_settings[i].pill_id) != 0)
        {
            has_change = 1;  // pill changed
        }else{
            if(_settings.pill_settings[i].pill_color != pill_settings->pill_settings[i].pill_color)
            {
                has_change = 1;  // color changed
            }
        }
    }
    xSemaphoreGive(_sync_mutex);

    if(!has_change)
    {
    	//LOGI("Pill settings not changed\n");
        return 1;
    }

    char* settings_file = PILL_SETTING_FILE;
    uint8_t buffer[MAX_BUFF_SIZE] = {0};
    pb_ostream_t sizestream = { 0 };
    if(!pb_encode(&sizestream, BatchedPillSettings_fields, pill_settings)){
        LOGE("Failed to encode pill settings: ");
        LOGE(PB_GET_ERROR(&sizestream));
        LOGE("\n");
        return 0;
    }

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if(!pb_encode(&stream, BatchedPillSettings_fields, pill_settings)){
        // we should never reach here.
        LOGE(PB_GET_ERROR(&stream));
        return 0;
    }

    if(0 != fs_save(settings_file, buffer, stream.bytes_written))
    {
        return 0;
    }

    xSemaphoreTake(_sync_mutex, portMAX_DELAY);
    memcpy(&_settings, pill_settings, sizeof(_settings));
    xSemaphoreGive(_sync_mutex);

    LOGI("pill settings saved\n");

    return true;

}
int pill_settings_load_from_file()
{
    char* settings_file = PILL_SETTING_FILE;
    uint8_t buffer[MAX_BUFF_SIZE] = {0};
    int bytes_read;

    int ret = fs_get( settings_file, (void*)buffer, sizeof(buffer), &bytes_read );
    if (ret != 0) {
        LOGE("failed to open file %s\n", settings_file);
        return 0;
    }
    LOGI("read %d bytes from file %s\n", bytes_read, settings_file);

    pb_istream_t stream = pb_istream_from_buffer(buffer, bytes_read);

    xSemaphoreTake(_sync_mutex, portMAX_DELAY);
    ret = pb_decode(&stream, BatchedPillSettings_fields, &_settings);
    xSemaphoreGive(_sync_mutex);

    if(!ret)
    {
        LOGE("Failed to decode pill settings\n");
        LOGE(PB_GET_ERROR(&stream));
        LOGE("\n");
        return 0;
    }

    LOGI("pill settings loaded\n");

    return 1;
}

int pill_settings_pill_count()
{
	int i = 0;
	int pill_count = 0;
	if(xSemaphoreTake(_sync_mutex, portMAX_DELAY))
	{
		for(i = 0; i < MAX_PILL_SETTINGS_COUNT; i++)
		{
			if(strlen(_settings.pill_settings[i].pill_id) > 0)
			{
				pill_count++;
			}
		}
		xSemaphoreGive(_sync_mutex);
	}

	return pill_count;
}


uint32_t pill_settings_get_color(const char* pill_id)
{
    int i = 0;
    if(xSemaphoreTake(_sync_mutex, portMAX_DELAY))
    {
        for(i = 0; i < MAX_PILL_SETTINGS_COUNT; i++)
        {
            if(strcmp(_settings.pill_settings[i].pill_id, pill_id) == 0)
            {
            	LOGI("%s color found %d\n", _settings.pill_settings[i].pill_id, _settings.pill_settings[i].pill_color);
            	xSemaphoreGive(_sync_mutex);
                return _settings.pill_settings[i].pill_color;
            }
        }
        xSemaphoreGive(_sync_mutex);
        LOGI("%s color not found\n", _settings.pill_settings[i].pill_id);
    }

    return 0;  // the default purple color
}

int pill_settings_init()
{
    _sync_mutex = xSemaphoreCreateMutex();
    int ret = pill_settings_load_from_file();
    int i = 0;
    for(i = 0; i < _settings.pill_settings_count; i++)
    {
    	LOGI("%s color loaded %d\n", _settings.pill_settings[i].pill_id, _settings.pill_settings[i].pill_color);
    }
    return ret;
}

int pill_settings_reset_all()
{
    xSemaphoreTake(_sync_mutex, portMAX_DELAY);
    memset(&_settings, 0, sizeof(_settings));
    xSemaphoreGive(_sync_mutex);

    unsigned long tok = 0;
    SlFsFileInfo_t info = {0};

    sl_FsGetInfo((unsigned char*)PILL_SETTING_FILE, tok, &info);
    int err = sl_FsDel((unsigned char*)PILL_SETTING_FILE, tok);
    if (err) {
        LOGI("error %d\n", err);
        return 0;
    }

    return 1;

}
