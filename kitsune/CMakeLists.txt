cmake_minimum_required (VERSION 2.8) 

project(EmbeddedAlgLibrary)

#set(CMAKE_INCLUDE_PATH ${PROJECT_SOURCE_DIR}/)

add_definitions(-DUSE_CPP_DEBUG_LOGGING)
add_definitions(-DUSED_ON_DESKTOP)
add_definitions(-DDESKTOP)


include_directories ("${PROJECT_SOURCE_DIR}")
include_directories ("${PROJECT_SOURCE_DIR}/nanopb")
include_directories ("${PROJECT_SOURCE_DIR}/protobuf")


#compile our alg code
add_library(EmbeddedAlg STATIC
hellomath.c
tensor/net_stats.c
nanopb/pb_decode.c
nanopb/pb_encode.c
nanopb/pb_common.c
protobuf/matrix.pb.c
debugutils/matmessageutils.c
debugutils/DebugLog.cpp
debugutils/base64.cpp
)


