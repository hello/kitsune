cmake_minimum_required (VERSION 2.8)

project(EmbeddedAlgLibrary)

#set(CMAKE_INCLUDE_PATH ${PROJECT_SOURCE_DIR}/)

add_definitions(-DUSE_CPP_DEBUG_LOGGING)
add_definitions(-DUSED_ON_DESKTOP)
add_definitions(-DDESKTOP)


include_directories ("nanopb")
include_directories ("protobuf")
include_directories ("../unit-test")


#compile our alg code
add_library(EmbeddedAlg STATIC
hellomath.c
audio_features_upload_task_helpers.c
hlo_stream.c
hlo_circbuf_stream.c
tensor/net_stats.c
nanopb/pb_decode.c
nanopb/pb_encode.c
nanopb/pb_common.c
protobuf/matrix.pb.c
debugutils/matmessageutils.c
debugutils/DebugLog.cpp
debugutils/base64.cpp
protobuf/async.pb.c
protobuf/audio_commands.pb.c
protobuf/audio_control.pb.c
protobuf/audio_data.pb.c
protobuf/file_manifest.pb.c
protobuf/filetransfer.pb.c
protobuf/keyword_stats.pb.c
protobuf/log.pb.c
protobuf/matrix.pb.c
protobuf/messeji.pb.c
protobuf/morpheus_ble.pb.c
protobuf/ntp.pb.c
protobuf/periodic.pb.c
protobuf/provision.pb.c
protobuf/response.pb.c
protobuf/simple_matrix.pb.c
protobuf/speech.pb.c
protobuf/state.pb.c
protobuf/streaming.pb.c
protobuf/sync_response.pb.c
)


