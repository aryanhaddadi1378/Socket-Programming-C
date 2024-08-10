cmake_minimum_required(VERSION 3.22)

project(Socket_Programming_C C)

set(CMAKE_C_STANDARD 11)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(SRC_DIR src)
set(INCLUDE_DIR include)

include_directories(${INCLUDE_DIR})

add_executable(client ${SRC_DIR}/client.c ${SRC_DIR}/utils.c)

add_executable(server ${SRC_DIR}/server.c ${SRC_DIR}/utils.c)
