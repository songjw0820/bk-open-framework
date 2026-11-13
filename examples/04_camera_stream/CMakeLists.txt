cmake_minimum_required(VERSION 3.16)
project(04_camera_stream C)

set(FRAMEWORK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_executable(04_camera_stream
    main.c
    "${FRAMEWORK_ROOT}/components/bk_core/src/bk_core.c"
    "${FRAMEWORK_ROOT}/components/bk_core/src/bk_event.c"
    "${FRAMEWORK_ROOT}/components/bk_board/src/bk_board.c"
    "${FRAMEWORK_ROOT}/components/bk_media/src/bk_media.c"
)

target_include_directories(04_camera_stream PRIVATE
    "${FRAMEWORK_ROOT}/components/bk_core/include"
    "${FRAMEWORK_ROOT}/components/bk_board/include"
    "${FRAMEWORK_ROOT}/components/bk_media/include"
    "${FRAMEWORK_ROOT}/boards/bk7259_robot"
)

target_compile_features(04_camera_stream PRIVATE c_std_11)

enable_testing()
add_test(NAME 04_camera_stream_test COMMAND 04_camera_stream)
