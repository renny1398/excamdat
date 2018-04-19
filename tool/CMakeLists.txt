cmake_minimum_required(VERSION 2.6)

project(tool)

set(CMAKE_C_FLAGS_DEBUG "-g -W -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "-g -W -Wall -std=c++11 -fsanitize=address")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -W -Wall -finline-functions -ffast-math")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -W -Wall -std=c++11 -finline-functions -ffast-math")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -W -Wall")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -W -Wall -std=c++11")
set(CMAKE_BUILD_TYPE RelWithDebInfo)

link_directories(/usr/local/lib ${CMAKE_SOURCE_DIR})
include_directories(/usr/include /usr/local/include ${CMAKE_SOURCE_DIR})

add_executable(create_drama_voice create_drama_voice.cc)
target_link_libraries(create_drama_voice mlib)

add_executable(exec7parser exec7parser.cc)
target_link_libraries(exec7parser mlib)

add_executable(routesim routesim.cc)
target_link_libraries(routesim mlib)

#include(FindPkgConfig)
#pkg_search_module(SDL2 REQUIRED sdl2)
#pkg_search_module(SDL2IMAGE REQUIRED SDL2_image>=2.0.0)
#include_directories(${SDL2_INCLUDE_DIRS} ${SDL2IMAGE_INCLUDE_DIRS})
#add_executable(dzi2png dzi2png.cpp)
#target_link_libraries(dzi2png ${SDL2_LIBRARIES} ${SDL2IMAGE_LIBRARIES})

