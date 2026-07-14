cmake_minimum_required(VERSION 3.20)
project(seavious C)

include(CTest)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(raylib CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

add_executable(seavious
    src/assets.c
    src/audio.c
    src/game_render.c
    src/game_state.c
    src/game_update.c
    src/gameplay.c
    src/main.c
    src/stage.c
    src/stage1_data.c
)

# TEMPORARY: a stale Windows-side file handle has the exact path
# build/Release/seavious.exe blocked (Access denied on creation, survives
# wsl --shutdown). Output under a different name until the next Windows
# reboot clears it, then this line can be removed.
set_target_properties(seavious PROPERTIES OUTPUT_NAME seavious-dev)

target_link_libraries(seavious PRIVATE raylib glfw)

if(BUILD_TESTING)
    add_executable(gameplay_tests
        tests/gameplay_tests.c
        src/gameplay.c
    )
    target_include_directories(gameplay_tests PRIVATE src)
    target_link_libraries(gameplay_tests PRIVATE raylib glfw)
    add_test(NAME gameplay_tests COMMAND gameplay_tests)

    add_executable(stage_tests
        tests/stage_tests.c
        src/game_state.c
        src/gameplay.c
        src/stage.c
        src/stage1_data.c
    )
    target_include_directories(stage_tests PRIVATE src)
    target_link_libraries(stage_tests PRIVATE raylib glfw)
    add_test(NAME stage_tests COMMAND stage_tests)

    if(UNIX)
        add_test(
            NAME game_smoke
            COMMAND xvfb-run -a ${CMAKE_COMMAND} -E env SEAVIOUS_SMOKE_FRAMES=480 $<TARGET_FILE:seavious>
        )
        set_tests_properties(game_smoke PROPERTIES WORKING_DIRECTORY $<TARGET_FILE_DIR:seavious>)
    endif()
endif()

add_custom_command(TARGET seavious POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:seavious>/assets
    COMMENT "Copying assets next to the built executable"
)

if(WIN32)
    target_link_libraries(seavious PRIVATE winmm)
endif()
