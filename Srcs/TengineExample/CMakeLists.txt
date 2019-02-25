if(CMAKE_TOOLCHAIN_FILE)
set(LIBRARY_OUTPUT_PATH_ROOT ${CMAKE_BINARY_DIR} CACHE PATH "root for library output, set this to change where android libs are compiled to")
# get absolute path, but get_filename_component ABSOLUTE only refer with source dir, so find_file here :(
get_filename_component(CMAKE_TOOLCHAIN_FILE_NAME ${CMAKE_TOOLCHAIN_FILE} NAME)
find_file(CMAKE_TOOLCHAIN_FILE ${CMAKE_TOOLCHAIN_FILE_NAME} PATHS ${CMAKE_SOURCE_DIR} NO_DEFAULT_PATH)
message(STATUS "CMAKE_TOOLCHAIN_FILE = ${CMAKE_TOOLCHAIN_FILE}")
endif()

if(NOT DEFINED CMAKE_INSTALL_PREFIX)
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "Installation Directory")
endif()
message(STATUS "CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}")

cmake_minimum_required(VERSION 2.8)
project(tm_classify)

add_definitions(-Wno-unused-command-line-argument)
add_definitions(-Wall)
add_definitions(-fPIC)
add_definitions(-g)
add_definitions(-O3)
add_definitions(-funroll-loops)
add_definitions(-Wno-overloaded-virtual)
add_definitions(-Wno-deprecated-register)
add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-std=c++11>)
set( TENGINE_DIR /usr/local/AID/Tengine/)
set( TENGINE_LIBS tengine )
set( CODE_SRCS tm_classification.cpp common.cpp )
set( BIN_EXE tm_classify )
set( ROCKCHIP_LIB /usr/lib64/librockchip_vendor.a )

find_package(OpenCV REQUIRED)

#include
include_directories(${TENGINE_DIR}/include
                    ${TENGINE_DIR}/example)

#lib
link_directories(${TENGINE_DIR}/lib)

#exe
add_executable(${BIN_EXE} ${CODE_SRCS})
target_link_libraries(${BIN_EXE} ${TENGINE_LIBS} ${ROCKCHIP_LIB} ${OpenCV_LIBS} pthread dl)
