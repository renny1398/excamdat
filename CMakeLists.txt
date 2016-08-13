cmake_minimum_required(VERSION 2.6)

PROJECT(excamdat)
SET(CMAKE_BUILD_TYPE Debug)
ADD_EXECUTABLE(excamdat main.cc lib.cc lib.h camellia.c)

INCLUDE_DIRECTORIES(/usr/include /usr/local/include)
LINK_DIRECTORIES(/usr/lib /usr/local/lib)
ADD_EXECUTABLE(setbgmloop setbgmloop.cc vcedit.c)
TARGET_LINK_LIBRARIES(setbgmloop ogg vorbis vorbisfile)
