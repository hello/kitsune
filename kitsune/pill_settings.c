#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "pill_settings.h"

#define MAX_BUFF_SIZE   128  // This is barely enough

static BatchedPillSettings _settings;
static xSemaphoreHandle _sync_mutex;

static int _write_to_file(const char* path, const char* buffer, size_t buffer_size)
{
    unsigned long tok = 0;
    long hndl = 0;
    SlFsFileInfo_t info = {0};

    sl_FsGetInfo((unsigned char*)path, tok, &info);

    if (sl_FsOpen((unsigned char*)path, FS_MODE_OPEN_WRITE, &tok, &hndl)) {
        LOGI("error opening file %s, trying to create\n", path);

        if (sl_FsOpen((unsigned char*)path, FS_MODE_OPEN_CREATE(65535, _FS_FILE_OPEN_FLAG_COMMIT), &tok,
                &hndl)) {
            LOGI("error opening %s for write\n", path);
            return 0;
        }else{
            sl_FsWrite(hndl, 0, buffer, buffer_size);  // Dummy write, we don't care about the result
        }
    }

    long bytes_written = sl_FsWrite(hndl, 0, buffer, buffer_size);
    if( bytes_written != buffer_size) {
        LOGE( "write pill settings failed %d", bytes_written);
    }
    sl_FsClose(hndl, 0, 0, 0);
    return bytes_written == buffer_size;
}

int pill_settings_save(const BatchedPillSettings* pill_settings)
{
    if(!pill_settings)
    {
        return 0;
    }

    int has_change = 0;
    int i, j;
    for(i = 0; i < MAX_PILL_COUNT; i++)
    {
        if(strcmp(_settings.pill_settings[i].pill_id, pill_settings->pill_settings[i].pill_id) != 0)
        {
            has_change = 1;  // pill changed
        }else{
            if(_settings.pill_settings[i].color != pill_settings->pill_settings[i].color)
            {
                has_change = 1;  // color changed
            }
        }
    }

    if(!has_change)
    {
        return 1;
    }

    const char* settings_file = PILL_SETTING_FILE;
    char buffer[MAX_BUFF_SIZE] = {0};
    pb_ostream_t sizestream = { 0 };
    if(!pb_encode(&sizestream, BatchedPillSettings_fields, pill_settings)){
        LOGE("Failed to encode pill settings: ");
        LOGE(PB_GET_ERROR(&stream));
        LOGE("\n");
        return 0;
    }

    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if(!pb_encode(&stream, BatchedPillSettings_fields, pill_settings)){
        // we should never reach here.
        LOGE(PB_GET_ERROR(&stream));
        return 0;
    }

    if(!_write_to_file(settings_file, buffer, stream.bytes_written))
    {
        return 0;
    }

    xSemaphoreTake(_sync_mutex, portMAX_DELAY);
    memcpy(&_settings, pill_settings, sizeof(_settings));
    xSemaphoreGive(_sync_mutex);

    return true;

}

int pill_settings_load_from_file()
{
    if(!pill_settings)
    {
        return 0;
    }
    const char* settings_file = PILL_SETTING_FILE;
    long file_handle = 0;

    // read in aes key
    int ret = sl_FsOpen(settings_file, FS_MODE_OPEN_READ, NULL, &file_handle);
    if (ret != 0) {
        LOGE("failed to open file %s\n", settings_file);
        return 0;
    }

    char buffer[MAX_BUFF_SIZE] = {0};
    long bytes_read = sl_FsRead(file_handle, 0, (unsigned char *)buffer, sizeof(buffer));
    LOGI("read %d bytes from file %s\n", bytes_read, settings_file);

    sl_FsClose(file_handle, NULL, NULL, 0);

    pb_istream_t stream = pb_istream_from_buffer(buffer, sizeof(buffer));

    xSemaphoreTake(_sync_mutex, portMAX_DELAY);
    ret = pb_decode(&stream, BatchedPillSettings_fields, &_settings);
    xSemaphoreGive(_sync_mutex);

    if(!ret)
    {
        LOGI("Failed to decode pill settings\n");
        LOGE(PB_GET_ERROR(&stream));
        LOGE("\n");
        return 0;
    }

    return 1;
}


uint32_t pill_settings_get_color(const char* pill_id)
{
    int i = 0;
    if(xSemaphoreTake(_sync_mutex, portMAX_DELAY))
    {
        for(i = 0; i < MAX_PILL_COUNT; i++)
        {
            if(strcmp(_settings.pill_settings[i].pill_id, pill_id) == 0)
            {
                return _settings.pill_settings[i].pill_id;
            }
        }
        xSemaphoreGive(_sync_mutex);
    }

    return 0xFF800080;
}

int pill_settings_init()
{
    _sync_mutex = xSemaphoreCreateMutex();
    pill_settings_load_from_file();
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