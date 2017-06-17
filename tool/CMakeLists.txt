cmake_minimum_required(VERSION 2.6)

project(tool)

link_directories(/usr/local/lib ../mlib)
include_directories(/usr/include /usr/local/include ..)
add_executable(create_drama_voice create_drama_voice.cc)
target_link_libraries(create_drama_voice mlib)

