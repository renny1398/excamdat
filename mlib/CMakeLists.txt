cmake_minimum_required(VERSION 2.6)

project(mlib)

include(FindPkgConfig)

pkg_search_module(SDL2 REQUIRED sdl2)
pkg_search_module(SDL2IMAGE REQUIRED SDL2_image>=2.0.0)

include_directories(${SDL2_INCLUDE_DIRS} ${SDL2IMAGE_INCLUDE_DIRS})

add_library(mlib camellia.c camellia.h cache.cc reader.cc mlib.cc extractor.cc exec.cc vmparser.cc cache.h reader.h mlib.h extractor.h exec.h vmparser.h)

find_path(CPPUNIT_INCLUDE_DIR cppunit/Test.h)
find_library(CPPUNIT_LIBRARY NAMES cppunit)

if (CPPUNIT_INCLUDE_DIR AND CPPUNIT_LIBRARY)
  set(CPPUNIT_FOUND TRUE)
  include_directories(${CPPUNIT_INCLUDE_DIR})
endif (CPPUNIT_INCLUDE_DIR AND CPPUNIT_LIBRARY)
