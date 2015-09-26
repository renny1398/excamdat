cmake_minimum_required(VERSION 2.6)

PROJECT(excamdat)
SET(CMAKE_BUILD_TYPE Debug)
ADD_EXECUTABLE(excamdat main.cc lib.cc lib.h camellia.c)
