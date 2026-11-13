cmake_minimum_required(VERSION 3.16)
project(05_peripheral_control C)

set(FRAMEWORK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_executable(05_peripheral_control
    main.c
    "${FRAMEWORK_ROOT}/components/bk_core/src/bk_core.c"
    "${FRAMEWORK_ROOT}/components/bk_core/src/bk_event.c"
    "${FRAMEWORK_ROOT}/components/bk_board/src/bk_board.c"
    "${FRAMEWORK_ROOT}/components/bk_peripheral/src/bk_peripheral.c"
)

target_include_directories(05_peripheral_control PRIVATE
    "${FRAMEWORK_ROOT}/components/bk_core/include"
    "${FRAMEWORK_ROOT}/components/bk_board/include"
    "${FRAMEWORK_ROOT}/components/bk_peripheral/include"
    "${FRAMEWORK_ROOT}/boards/bk7259_robot"
)

target_compile_features(05_peripheral_control PRIVATE c_std_11)

enable_testing()
add_test(NAME 05_peripheral_control_test COMMAND 05_peripheral_control)
