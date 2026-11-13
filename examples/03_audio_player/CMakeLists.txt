cmake_minimum_required(VERSION 3.16)
project(03_audio_player C)

set(FRAMEWORK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_executable(03_audio_player
    main.c
    "${FRAMEWORK_ROOT}/components/bk_core/src/bk_core.c"
    "${FRAMEWORK_ROOT}/components/bk_core/src/bk_event.c"
    "${FRAMEWORK_ROOT}/components/bk_board/src/bk_board.c"
    "${FRAMEWORK_ROOT}/components/bk_media/src/bk_media.c"
)

target_include_directories(03_audio_player PRIVATE
    "${FRAMEWORK_ROOT}/components/bk_core/include"
    "${FRAMEWORK_ROOT}/components/bk_board/include"
    "${FRAMEWORK_ROOT}/components/bk_media/include"
    "${FRAMEWORK_ROOT}/boards/bk7259_robot"
)

target_compile_features(03_audio_player PRIVATE c_std_11)

enable_testing()
add_test(NAME 03_audio_player_test COMMAND 03_audio_player)
