cmake_minimum_required(VERSION 2.6)

project(test)

find_path(CPPUNIT_INCLUDE_DIR cppunit/Test.h)
find_library(CPPUNIT_LIBRARY NAMES cppunit)

if (CPPUNIT_INCLUDE_DIR AND CPPUNIT_LIBRARY)
  set(CPPUNIT_FOUND TRUE)
  include_directories(${CPPUNIT_INCLUDE_DIR})
  enable_testing()
endif (CPPUNIT_INCLUDE_DIR AND CPPUNIT_LIBRARY)

include_directories(..)
add_executable(reader_test reader_test.cc)
target_link_libraries(reader_test ${CPPUNIT_LIBRARY} mlib)
add_test(NAME reader COMMAND $<TARGET_FILE:reader_test>)

add_executable(slt_test slt_test.cc)
