cmake_minimum_required(VERSION 3.31)
project(VirtualMemoryManager C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)


add_executable(VirtualMemoryManager vm.c
        components/utilities.c
        components/utilities.h
        components/initialize.c
        components/initialize.h
        components/pages.c
        components/pages.h
        components/disk.c
        components/disk.h)