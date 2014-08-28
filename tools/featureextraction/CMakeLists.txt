cmake_minimum_required (VERSION 2.8) 

PROJECT (PcmRunner)

set(CMAKE_INCLUDE_PATH ${PROJECT_SOURCE_DIR}/)

add_definitions(-DUSE_CPP_DEBUG_LOGGING)


#compile our alg code
add_library(EmbeddedAlg STATIC
../../kitsune/fft.c
../../kitsune/audiofeatures.c
../../kitsune/nanopb/pb_decode.c
../../kitsune/nanopb/pb_encode.c
../../kitsune/protobuf/matrix.pb.c
../../kitsune/debugutils/matmessageutils.c
../../kitsune/debugutils/DebugLog.cpp
../../kitsune/debugutils/base64.cpp
)




add_executable(pcmrunner
               pcmrunner.cpp)

target_link_libraries(pcmrunner EmbeddedAlg)

