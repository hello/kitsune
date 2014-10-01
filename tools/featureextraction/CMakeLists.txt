cmake_minimum_required (VERSION 2.8) 

PROJECT (PcmRunner)

set(CMAKE_INCLUDE_PATH ${PROJECT_SOURCE_DIR}/)

add_definitions(-DUSE_CPP_DEBUG_LOGGING)

include_directories ("${PROJECT_SOURCE_DIR}/../../nanopb")


add_subdirectory("../../kitsune" "${CMAKE_CURRENT_BINARY_DIR}/kitsune")


add_executable(pcmrunner
               pcmrunner.cpp)

target_link_libraries(pcmrunner EmbeddedAlg)

