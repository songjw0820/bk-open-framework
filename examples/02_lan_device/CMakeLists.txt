cmake_minimum_required(VERSION 3.16)
project(02_lan_device C)

set(FRAMEWORK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_executable(02_lan_device
    main.c
    "${FRAMEWORK_ROOT}/components/bk_core/src/bk_core.c"
    "${FRAMEWORK_ROOT}/components/bk_core/src/bk_event.c"
    "${FRAMEWORK_ROOT}/components/bk_board/src/bk_board.c"
    "${FRAMEWORK_ROOT}/components/bk_net/src/bk_net.c"
    "${FRAMEWORK_ROOT}/components/bk_net/src/bk_provision.c"
    "${FRAMEWORK_ROOT}/components/bk_net/src/bk_lan.c"
)

target_include_directories(02_lan_device PRIVATE
    "${FRAMEWORK_ROOT}/components/bk_core/include"
    "${FRAMEWORK_ROOT}/components/bk_board/include"
    "${FRAMEWORK_ROOT}/components/bk_net/include"
    "${FRAMEWORK_ROOT}/boards/bk7259_robot"
)

target_compile_features(02_lan_device PRIVATE c_std_11)

enable_testing()
add_test(NAME 02_lan_device_test COMMAND 02_lan_device)
