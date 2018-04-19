cmake_minimum_required(VERSION 2.6)

project(exmaldat)

set(CMAKE_C_FLAGS_DEBUG "-g -W -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "-g -W -Wall -std=c++11")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -W -Wall -finline-functions -ffast-math")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -W -Wall -std=c++11 -finline-functions -ffast-math")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g -W -Wall")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -W -Wall -std=c++11")

set(CMAKE_BUILD_TYPE Debug)
#set(CMAKE_BUILD_TYPE RelWithDebInfo)

add_subdirectory(mlib)
add_subdirectory(PeLib)
add_subdirectory(tool)
add_subdirectory(test)

link_directories(/usr/local/lib)
add_executable(exmaldat exmaldat.cc)
target_link_libraries(exmaldat mlib ${SDL2_LIBRARIES} ${SDL2IMAGE_LIBRARIES})

include_directories(/usr/include /usr/local/include)
link_directories(/usr/lib /usr/local/lib)
#add_executable(setbgmloop setbgmloop.cc vcedit.c)
#target_link_libraries(setbgmloop ogg vorbis vorbisfile)

find_package(zlib)
if(ZLIB_FOUND)
    include_directories(${ZLIB_INCLUDE_DIRS})
    #add_executable(exexecdat exexecdat.cc mlib/camellia.c mlib/camellia.h)
    #target_link_libraries(exexecdat PeLib ${ZLIB_LIBRARIES})
endif(ZLIB_FOUND)
