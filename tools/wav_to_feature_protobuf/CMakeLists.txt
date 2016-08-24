cmake_minimum_required (VERSION 2.8) 

PROJECT (WavRunner)

set(CMAKE_INCLUDE_PATH ${PROJECT_SOURCE_DIR}/)
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

find_package(SndFile REQUIRED)

add_definitions(-DUSE_CPP_DEBUG_LOGGING)
add_definitions(-DNO_EQUALIZATION)

include_directories ("${PROJECT_SOURCE_DIR}/../../kitsune/nanopb" ${SNDFILE_INCLUDE_DIRS})

message(STATUS "${SNDFILE_INCLUDE_DIRS}")
message(STATUS "${SNDFILE_LIBRARIES}")


add_subdirectory("../../kitsune" "${CMAKE_CURRENT_BINARY_DIR}/kitsune")


add_executable(wavrunner
               wavrunner.cpp)

target_link_libraries(wavrunner EmbeddedAlg ${SNDFILE_LIBRARIES})

