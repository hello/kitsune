
#include "ble_cmd.h"

#include "wifi_cmd.h"


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


void free_protobuf_command(const MorpheusCommand* command)
{
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