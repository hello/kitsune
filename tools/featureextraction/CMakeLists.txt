cmake_minimum_required (VERSION 2.8) 

PROJECT (PcmRunner)

set(CMAKE_INCLUDE_PATH ${PROJECT_SOURCE_DIR}/)

#compile our alg code
add_library(EmbeddedAlg STATIC
../../kitsune/fft.c
../../kitsune/audiofeatures.c
../../kitsune/nanopb/pb_decode.c
../../kitsune/nanopb/pb_encode.c
../../kitsune/protobuf/matrix.pb.c
../../kitsune/matmessageutils.c
)


add_executable(pcmrunner
               pcmrunner.cpp
               base64.cpp)

target_link_libraries(pcmrunner EmbeddedAlg)

