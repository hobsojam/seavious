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
    src/boss.c
    src/game_render.c
    src/game_state.c
    src/game_update.c
    src/gameplay.c
    src/input.c
    src/main.c
    src/menu.c
    src/present.c
    src/screenshot.c
    src/settings.c
    src/stage.c
    src/stage1_data.c
    src/stage2_data.c
    src/title.c
    src/wreck_visual.c
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
        src/stage2_data.c
        src/wreck_visual.c
    )
    target_include_directories(stage_tests PRIVATE src)
    target_link_libraries(stage_tests PRIVATE raylib glfw)
    add_test(NAME stage_tests COMMAND stage_tests)

    add_executable(title_tests
        tests/title_tests.c
        src/gameplay.c
        src/input.c
        src/menu.c
        src/present.c
        src/settings.c
        src/title.c
    )
    target_include_directories(title_tests PRIVATE src)
    target_link_libraries(title_tests PRIVATE raylib glfw)
    add_test(NAME title_tests COMMAND title_tests)

    add_executable(settings_tests
        tests/settings_tests.c
        src/input.c
        src/settings.c
    )
    target_include_directories(settings_tests PRIVATE src)
    target_link_libraries(settings_tests PRIVATE raylib glfw)
    add_test(NAME settings_tests COMMAND settings_tests)

    add_executable(screenshot_tests
        tests/screenshot_tests.c
        src/screenshot.c
    )
    target_include_directories(screenshot_tests PRIVATE src)
    add_test(NAME screenshot_tests COMMAND screenshot_tests)

    add_executable(boss_tests
        tests/boss_tests.c
        src/boss.c
        src/game_state.c
        src/gameplay.c
        src/stage.c
        src/stage1_data.c
        src/stage2_data.c
    )
    target_include_directories(boss_tests PRIVATE src)
    target_link_libraries(boss_tests PRIVATE raylib glfw)
    add_test(NAME boss_tests COMMAND boss_tests)

    if(UNIX)
        add_test(
            NAME game_smoke
            COMMAND xvfb-run -a ${CMAKE_COMMAND} -E env SEAVIOUS_SMOKE_FRAMES=480 $<TARGET_FILE:seavious>
        )
        set_tests_properties(game_smoke PROPERTIES WORKING_DIRECTORY $<TARGET_FILE_DIR:seavious>)
    endif()
endif()

add_custom_command(TARGET seavious POST_BUILD
    # copy_directory only compares timestamps.  A zero-byte asset left by
    # an interrupted Windows/WSL build can therefore be treated as current
    # and shipped beside the executable.  The script compares contents and
    # replaces those placeholders, while retaining the asset subdirectories.
    COMMAND ${CMAKE_COMMAND}
        -DASSET_SOURCE_DIR=${CMAKE_SOURCE_DIR}/assets
        -DASSET_DEST_DIR=$<TARGET_FILE_DIR:seavious>/assets
        -P ${CMAKE_SOURCE_DIR}/tools/copy-assets.cmake
    COMMENT "Copying assets next to the built executable"
)

if(WIN32)
    target_link_libraries(seavious PRIVATE winmm)
    # Embeds the generated application icon (see tools/gen-exe-icon.py).
    enable_language(RC)
    target_sources(seavious PRIVATE src/seavious.rc)
endif()
