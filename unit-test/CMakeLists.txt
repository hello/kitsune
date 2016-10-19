cmake_minimum_required (VERSION 2.8) 

PROJECT (EmbeddedAlgTests)

set(CMAKE_INCLUDE_PATH ${PROJECT_SOURCE_DIR}/)
#set(CMAKE_OSX_DEPLOYMENT_TARGET "10.12")

find_package(Protobuf REQUIRED)
include_directories ("${PROTOBUF_INCLUDE_DIRS}")
include_directories("../kitsune/nanopb")

#compile gtest
add_subdirectory("../third_party/gtest" "${CMAKE_CURRENT_BINARY_DIR}/gtest" )

#compile algs
add_subdirectory("../kitsune/" "${CMAKE_CURRENT_BINARY_DIR}/kitsune")

#compile unit tests
include_directories("../third_party/gtest/include")
include_directories ("../kitsune/")

#protobuf definition in C++
add_library(cpp-protobuf-defs STATIC
cpp_proto/keyword_stats.pb.cc
)

add_executable(unit-test
               main.cpp 
	       testmath.cpp
           testnetstats.cpp)

target_link_libraries(unit-test gtest EmbeddedAlg cpp-protobuf-defs ${PROTOBUF_LIBRARY})

