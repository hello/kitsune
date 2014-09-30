#include <string.h>

#include "ble_cmd.h"

#include "wifi_cmd.h"

static bool _encode_string_fields(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    char* str = *arg;
    if(!str)
    {
        return false;
    }
    
    if (!pb_encode_tag_for_field(stream, field))
    {
        return false;
    }

    return pb_encode_string(stream, (uint8_t*)str, strlen(str));
}


static bool _decode_string_field(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
    /* We could read block-by-block to avoid the large buffer... */
    if (stream->bytes_left > MAX_STRING_LEN - 1)
    {
        return false;
    }
    
    char* str = malloc(stream->bytes_left + 1);
    if(!str)
    {
        return false;
    }

    memset(str, 0, stream->bytes_left + 1);
    if (!pb_read(stream, str, stream->bytes_left))
    {
        return false;
    }
    *arg = str;

    return true;
}

static void _on_ble_protobuf_command(const MorpheusCommand* command)
{
    switch(command->type)
    {
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SET_WIFI_ENDPOINT:
        {
            const char* ssid = command->wifiSSID.arg;  // Mac Address in String:  xx:xx:xx:xx
            const char* password = command->wifiPassword.arg;
            // Just call API to connect to WIFI.
            UARTprintf("Wifi SSID %s, pswd %s \r\n", ssid, password);  // might print junk.
            // Just simulate the command line input.
            // If the command line works, this shit should work.
            char* args[4];
            memset(args, 0, sizeof(args));
            args[1] = ssid;
            args[2] = password;
            args[3] = strlen(password) == 0 ? "0" : "2";  // TODO: Guess the security type.

            // This is just a hack to make the Wifi connect
            // TODO: check with the connection status and loop through for 
            // different security type.
            Cmd_connect(4, args);
        }
        break;
        case MorpheusCommand_CommandType_MORPHEUS_COMMAND_SWITCH_TO_PAIRING_MODE:
        {
            // Light up LEDs?
        }
        break;
    }
}


void on_morpheus_protobuf_arrival(const char* protobuf, size_t len)
{
    if(!protobuf)
    {
        UARTprintf("Inavlid parameter.\r\n");
        return;
    }

    MorpheusCommand command;
    memset(&command, 0, sizeof(command));

    
    command.accountId.funcs.decode = _decode_string_field;
    command.accountId.arg = NULL;

    command.deviceId.funcs.decode = _decode_string_field;
    command.deviceId.arg = NULL;

    command.wifiName.funcs.decode = _decode_string_field;
    command.wifiName.arg = NULL;

    command.wifiSSID.funcs.decode = _decode_string_field;
    command.wifiSSID.arg = NULL;

    command.wifiPassword.funcs.decode = _decode_string_field;
    command.wifiPassword.arg = NULL;

    pb_istream_t stream = pb_istream_from_buffer(protobuf, len);
    bool status = pb_decode(&stream, MorpheusCommand_fields, &command);
    

    if (!status)
    {
        UARTprintf("Decoding protobuf failed, error: ");
        UARTprintf(PB_GET_ERROR(&stream));
        UARTprintf("\r\n");
    }else{

        _on_ble_protobuf_command(&command);
    }

    free_protobuf_command(&command);
    
}

static MorpheusCommand* _assign_encode_funcs(MorpheusCommand* command)
{
    if(command->accountId.arg != NULL && command->accountId.funcs.encode == NULL)
    {
        command->accountId.funcs.encode = _encode_string_fields;
    }

    if(command->deviceId.arg != NULL && command->deviceId.funcs.encode == NULL)
    {
        command->deviceId.funcs.encode = _encode_string_fields;
    }

    if(command->wifiName.arg != NULL && command->wifiName.funcs.encode == NULL)
    {
        command->wifiName.funcs.encode = _encode_string_fields;
    }

    if(command->wifiSSID.arg != NULL && command->wifiSSID.funcs.encode == NULL)
    {
        command->wifiSSID.funcs.encode = _encode_string_fields;
    }

    if(command->wifiPassword.arg != NULL && command->wifiPassword.funcs.encode == NULL)
    {
        command->wifiPassword.funcs.encode = _encode_string_fields;
    }

    return command;
}

bool send_protobuf_to_ble(MorpheusCommand* command)
{
    if(!command){
        UARTprintf("Inavlid parameter.\r\n");
        return false;
    }


    _assign_encode_funcs(command);


    pb_ostream_t stream = {0};
    pb_encode(&stream, MorpheusCommand_fields, command);

    char* heap_page = malloc(stream.bytes_written);
    if(!heap_page)
    {
        UARTprintf("Not enough memory.\r\n");
        return false;
    }

    memset(heap_page, 0, stream.bytes_written);
    stream = pb_ostream_from_buffer(heap_page, stream.bytes_written);

    bool status = pb_encode(&stream, MorpheusCommand_fields, command);
    
    if(status)
    {
        size_t protobuf_len = stream.bytes_written;
        spi_write(protobuf_len, heap_page);

    }else{
        UARTprintf("encode protobuf failed: ");
        UARTprintf(PB_GET_ERROR(&stream));
        UARTprintf("\r\n");
    }
    free(heap_page);

    return status;
}


void free_protobuf_command(const MorpheusCommand* command)
{
    if(!command)
    {
        UARTprintf("Inavlid parameter.\r\n");
        return;
    }

    if(!command->accountId.arg)
    {
        free(command->accountId.arg);
        command->accountId.arg = NULL;
    }

    if(!command->deviceId.arg)
    {
        free(command->deviceId.arg);
        command->deviceId.arg = NULL;
    }

    if(!command->wifiName.arg)
    {
        free(command->wifiName.arg);
        command->wifiName.arg = NULL;
    }

    if(!command->wifiSSID.arg)
    {
        free(command->wifiSSID.arg);
        command->wifiSSID.arg = NULL;
    }

    if(!command->wifiPassword.arg)
    {
        free(command->wifiPassword.arg);
        command->wifiPassword.arg = NULL;
    }
}