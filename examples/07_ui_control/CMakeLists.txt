cmake_minimum_required(VERSION 3.16)
project(07_ui_control C)

set(FRAMEWORK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_executable(07_ui_control
    main.c
    "${FRAMEWORK_ROOT}/components/bk_core/src/bk_core.c"
    "${FRAMEWORK_ROOT}/components/bk_core/src/bk_event.c"
    "${FRAMEWORK_ROOT}/components/bk_board/src/bk_board.c"
    "${FRAMEWORK_ROOT}/components/bk_ui/src/bk_ui.c"
)

target_include_directories(07_ui_control PRIVATE
    "${FRAMEWORK_ROOT}/components/bk_core/include"
    "${FRAMEWORK_ROOT}/components/bk_board/include"
    "${FRAMEWORK_ROOT}/components/bk_ui/include"
    "${FRAMEWORK_ROOT}/boards/bk7259_robot"
)

target_compile_features(07_ui_control PRIVATE c_std_11)

enable_testing()
add_test(NAME 07_ui_control_test COMMAND 07_ui_control)
