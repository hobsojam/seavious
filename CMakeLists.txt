cmake_minimum_required(VERSION 3.20)
project(seavious C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(raylib CONFIG REQUIRED)

add_executable(seavious
    src/main.c
)

target_link_libraries(seavious PRIVATE raylib)

if(WIN32)
    target_link_libraries(seavious PRIVATE winmm)
endif()
