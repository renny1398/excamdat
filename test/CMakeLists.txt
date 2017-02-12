cmake_minimum_required(VERSION 2.6)

project(test)

find_path(CPPUNIT_INCLUDE_DIR cppunit/Test.h)
find_library(CPPUNIT_LIBRARY NAMES cppunit)

if (CPPUNIT_INCLUDE_DIR AND CPPUNIT_LIBRARY)
  set(CPPUNIT_FOUND TRUE)
  include_directories(${CPPUNIT_INCLUDE_DIR})
endif (CPPUNIT_INCLUDE_DIR AND CPPUNIT_LIBRARY)

add_executable(slt_test slt_test.cc)
