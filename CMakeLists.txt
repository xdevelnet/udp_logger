cmake_minimum_required(VERSION 3.26)
project(udp_logger C)

set(CMAKE_C_STANDARD 99)

add_executable(udp_logger main.c)
target_link_libraries(udp_logger m)
