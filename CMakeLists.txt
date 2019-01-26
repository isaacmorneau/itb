cmake_minimum_required (VERSION 2.6)
project (itb)

set (itb_VERSION_MAJOR 1)
set (itb_VERSION_MINOR 0)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()


set(CMAKE_C_FLAGS "-Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "-g -O0 -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_C_FLAGS_RELEASE "-Ofast -s -fno-ident  -march=native -flto -DNDEBUG")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}")

add_definitions(-D_POSIX_C_SOURCE=200809L)
add_definitions(-D_DEFAULT_SOURCE)

SET(SOURCES
    "itb_testing.c"
    )

SET(RAW_UI_SOURCES
    "itb_ui_testing.c"
    )

add_executable(itb ${SOURCES})

add_executable(itb_ui ${RAW_UI_SOURCES})

if (CMAKE_BUILD_TYPE EQUAL Release)
    set_target_properties(itb PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE POSITION_INDEPENDENT_CODE TRUE)
    set_target_properties(itb_ui PROPERTIES INTERPROCEDURAL_OPTIMIZATION TRUE POSITION_INDEPENDENT_CODE TRUE)
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

target_link_libraries(itb rt Threads::Threads mbedtls mbedx509 mbedcrypto)
target_link_libraries(itb_ui rt Threads::Threads)
