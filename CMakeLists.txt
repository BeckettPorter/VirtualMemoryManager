cmake_minimum_required(VERSION 3.31)
project(VirtualMemoryManager C)

set(CMAKE_C_STANDARD 11)

add_executable(VirtualMemoryManager vm.c
        components/utilities.c
        components/utilities.h)