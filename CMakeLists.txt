cmake_minimum_required(VERSION 3.16)
project(OS_ex1 C)

set(CMAKE_C_STANDARD 11)

add_executable(OS_ex1 ex1.c)

file(COPY t.sh DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
