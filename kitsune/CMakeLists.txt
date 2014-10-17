cmake_minimum_required (VERSION 2.8) 

project(EmbeddedAlgLibrary)

#set(CMAKE_INCLUDE_PATH ${PROJECT_SOURCE_DIR}/)

add_definitions(-DUSE_CPP_DEBUG_LOGGING)

include_directories ("${PROJECT_SOURCE_DIR}")
include_directories ("${PROJECT_SOURCE_DIR}/nanopb")


#compile our alg code
add_library(EmbeddedAlg STATIC
hellomath.c
fft.c
audiofeatures.c
audioclassifier.c
nanopb/pb_decode.c
nanopb/pb_encode.c
protobuf/matrix.pb.c
debugutils/matmessageutils.c
debugutils/DebugLog.cpp
debugutils/base64.cpp
machinelearning/linear_svm.c
machinelearning/classifier_factory.c
machinelearning/hmm.c
)


