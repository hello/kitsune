cmake_minimum_required (VERSION 2.8) 

PROJECT (EmbeddedAlgTests)

set(CMAKE_INCLUDE_PATH ${PROJECT_SOURCE_DIR}/)

add_subdirectory("../third_party/gtest" "${CMAKE_CURRENT_BINARY_DIR}/gtest" )



include_directories("../third_party/gtest/include")

add_executable(unit-test
               main.cpp 
               testfft.cpp)

target_link_libraries(unit-test gtest)

