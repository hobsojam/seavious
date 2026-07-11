cmake_minimum_required(VERSION 3.20)
project(seavious C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(raylib CONFIG REQUIRED)

add_executable(seavious
    src/main.c
)

target_link_libraries(seavious PRIVATE raylib)

add_custom_command(TARGET seavious POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:seavious>/assets
    COMMENT "Copying assets next to the built executable"
)

if(WIN32)
    target_link_libraries(seavious PRIVATE winmm)
endif()
