cmake_minimum_required(VERSION 2.6)

project(PeLib)

set(CMAKE_C_FLAGS_DEBUG "-g -W -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "-g -W -Wall")
set(CMAKE_C_FLAGS_RELEASE "-Os -W -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -W -Wall")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -W -Wall")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -W -Wall")

set(CMAKE_BUILD_TYPE Release)

file(GLOB HDR *.h)
file(GLOB SRC *.cpp)
file(GLOB HDR_BUFFER buffer/*.cpp)
file(GLOB SRC_BUFFER buffer/*.cpp)

add_library(PeLib ${SRC} ${HDR} ${SRC_BUFFER} ${HDR_BUFFER})
