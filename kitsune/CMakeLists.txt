cmake_minimum_required (VERSION 2.8) 

project(EmbeddedAlgLibrary)

#set(CMAKE_INCLUDE_PATH ${PROJECT_SOURCE_DIR}/)

add_definitions(-DUSE_CPP_DEBUG_LOGGING)
add_definitions(-DUSED_ON_DESKTOP)


include_directories ("${PROJECT_SOURCE_DIR}")
include_directories ("${PROJECT_SOURCE_DIR}/nanopb")
include_directories ("${PROJECT_SOURCE_DIR}/protobuf")


#compile our alg code
add_library(EmbeddedAlg STATIC
hellomath.c
fft.c
rawaudiostatemachine.c
audiofeatures.c
audioclassifier.c
nanopb/pb_decode.c
nanopb/pb_encode.c
nanopb/pb_common.c
protobuf/matrix.pb.c
protobuf/
debugutils/matmessageutils.c
debugutils/DebugLog.cpp
debugutils/base64.cpp
machinelearning/linear_svm.c
machinelearning/classifier_factory.c
machinelearning/hmm.c
machinelearning/audiohmm.c
)


