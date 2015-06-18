cmake_minimum_required (VERSION 2.8) 

PROJECT (EmbeddedAlgTests)

set(CMAKE_INCLUDE_PATH ${PROJECT_SOURCE_DIR}/)



#compile gtest
add_subdirectory("../third_party/gtest" "${CMAKE_CURRENT_BINARY_DIR}/gtest" )

#compile algs
add_subdirectory("../kitsune/" "${CMAKE_CURRENT_BINARY_DIR}/kitsune")

#compile unit tests
include_directories("../third_party/gtest/include")

add_executable(unit-test
               main.cpp 
               testfft.cpp
	       testmath.cpp
               testprox.cpp)

target_link_libraries(unit-test gtest EmbeddedAlg)

