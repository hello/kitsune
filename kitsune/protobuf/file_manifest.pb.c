/* Automatically generated nanopb constant definitions */
/* Generated by nanopb-0.3.1 at Tue Mar 15 12:38:15 2016. */

#include "file_manifest.pb.h"

#if PB_PROTO_HEADER_VERSION != 30
#error Regenerate this file with the current version of nanopb generator.
#endif



const pb_field_t FileManifest_fields[11] = {
    PB_FIELD(  1, MESSAGE , REPEATED, CALLBACK, FIRST, FileManifest, file_info, file_info, &FileManifest_File_fields),
    PB_FIELD(  2, ENUM    , OPTIONAL, STATIC  , OTHER, FileManifest, file_status, file_info, 0),
    PB_FIELD(  3, UINT32  , OPTIONAL, STATIC  , OTHER, FileManifest, device_uptime_in_seconds, file_status, 0),
    PB_FIELD(  4, UINT32  , OPTIONAL, STATIC  , OTHER, FileManifest, query_delay, device_uptime_in_seconds, 0),
    PB_FIELD(  5, INT32   , OPTIONAL, STATIC  , OTHER, FileManifest, firmware_version, query_delay, 0),
    PB_FIELD(  6, INT32   , OPTIONAL, STATIC  , OTHER, FileManifest, unix_time, firmware_version, 0),
    PB_FIELD(  7, MESSAGE , OPTIONAL, STATIC  , OTHER, FileManifest, link_health, unix_time, &FileManifest_LinkHealth_fields),
    PB_FIELD(  8, MESSAGE , OPTIONAL, STATIC  , OTHER, FileManifest, sd_card_size, link_health, &FileManifest_MemoryInfo_fields),
    PB_FIELD(  9, MESSAGE , REPEATED, STATIC  , OTHER, FileManifest, error_info, sd_card_size, &FileManifest_FileOperationError_fields),
    PB_FIELD( 10, STRING  , OPTIONAL, CALLBACK, OTHER, FileManifest, sense_id, error_info, 0),
    PB_LAST_FIELD
};

const pb_field_t FileManifest_FileDownload_fields[6] = {
    PB_FIELD(  1, STRING  , OPTIONAL, CALLBACK, FIRST, FileManifest_FileDownload, host, host, 0),
    PB_FIELD(  2, STRING  , OPTIONAL, CALLBACK, OTHER, FileManifest_FileDownload, url, host, 0),
    PB_FIELD(  3, STRING  , OPTIONAL, CALLBACK, OTHER, FileManifest_FileDownload, sd_card_filename, url, 0),
    PB_FIELD(  4, STRING  , OPTIONAL, CALLBACK, OTHER, FileManifest_FileDownload, sd_card_path, sd_card_filename, 0),
    PB_FIELD(  5, BYTES   , OPTIONAL, STATIC  , OTHER, FileManifest_FileDownload, sha1, sd_card_path, 0),
    PB_LAST_FIELD
};

const pb_field_t FileManifest_FileOperationError_fields[4] = {
    PB_FIELD(  1, STRING  , OPTIONAL, CALLBACK, FIRST, FileManifest_FileOperationError, filename, filename, 0),
    PB_FIELD(  2, ENUM    , OPTIONAL, STATIC  , OTHER, FileManifest_FileOperationError, err_type, filename, 0),
    PB_FIELD(  3, INT32   , OPTIONAL, STATIC  , OTHER, FileManifest_FileOperationError, err_code, err_type, 0),
    PB_LAST_FIELD
};

const pb_field_t FileManifest_File_fields[4] = {
    PB_FIELD(  1, MESSAGE , OPTIONAL, STATIC  , FIRST, FileManifest_File, download_info, download_info, &FileManifest_FileDownload_fields),
    PB_FIELD(  2, BOOL    , OPTIONAL, STATIC  , OTHER, FileManifest_File, delete_file, download_info, 0),
    PB_FIELD(  3, BOOL    , OPTIONAL, STATIC  , OTHER, FileManifest_File, update_file, delete_file, 0),
    PB_LAST_FIELD
};

const pb_field_t FileManifest_LinkHealth_fields[3] = {
    PB_FIELD(  1, UINT32  , OPTIONAL, STATIC  , FIRST, FileManifest_LinkHealth, send_errors, send_errors, 0),
    PB_FIELD(  2, UINT32  , OPTIONAL, STATIC  , OTHER, FileManifest_LinkHealth, time_to_response, send_errors, 0),
    PB_LAST_FIELD
};

const pb_field_t FileManifest_MemoryInfo_fields[4] = {
    PB_FIELD(  1, UINT32  , OPTIONAL, STATIC  , FIRST, FileManifest_MemoryInfo, total_memory, total_memory, 0),
    PB_FIELD(  2, UINT32  , OPTIONAL, STATIC  , OTHER, FileManifest_MemoryInfo, free_memory, total_memory, 0),
    PB_FIELD(  3, BOOL    , OPTIONAL, STATIC  , OTHER, FileManifest_MemoryInfo, sd_card_failure, free_memory, 0),
    PB_LAST_FIELD
};


/* Check that field information fits in pb_field_t */
#if !defined(PB_FIELD_32BIT)
/* If you get an error here, it means that you need to define PB_FIELD_32BIT
 * compile-time option. You can do that in pb.h or on compiler command line.
 * 
 * The reason you need to do this is that some of your messages contain tag
 * numbers or field sizes that are larger than what can fit in 8 or 16 bit
 * field descriptors.
 */
PB_STATIC_ASSERT((pb_membersize(FileManifest, file_info) < 65536 && pb_membersize(FileManifest, link_health) < 65536 && pb_membersize(FileManifest, sd_card_size) < 65536 && pb_membersize(FileManifest, error_info[0]) < 65536 && pb_membersize(FileManifest_File, download_info) < 65536), YOU_MUST_DEFINE_PB_FIELD_32BIT_FOR_MESSAGES_FileManifest_FileManifest_FileDownload_FileManifest_FileOperationError_FileManifest_File_FileManifest_LinkHealth_FileManifest_MemoryInfo)
#endif

#if !defined(PB_FIELD_16BIT) && !defined(PB_FIELD_32BIT)
/* If you get an error here, it means that you need to define PB_FIELD_16BIT
 * compile-time option. You can do that in pb.h or on compiler command line.
 * 
 * The reason you need to do this is that some of your messages contain tag
 * numbers or field sizes that are larger than what can fit in the default
 * 8 bit descriptors.
 */
PB_STATIC_ASSERT((pb_membersize(FileManifest, file_info) < 256 && pb_membersize(FileManifest, link_health) < 256 && pb_membersize(FileManifest, sd_card_size) < 256 && pb_membersize(FileManifest, error_info[0]) < 256 && pb_membersize(FileManifest_File, download_info) < 256), YOU_MUST_DEFINE_PB_FIELD_16BIT_FOR_MESSAGES_FileManifest_FileManifest_FileDownload_FileManifest_FileOperationError_FileManifest_File_FileManifest_LinkHealth_FileManifest_MemoryInfo)
#endif


