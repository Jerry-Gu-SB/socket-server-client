cmake_minimum_required(VERSION 3.22)
project(please_work C)

set(CMAKE_C_STANDARD 11)

add_executable(please_work sclient.c sserver.c)
