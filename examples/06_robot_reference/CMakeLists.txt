cmake_minimum_required(VERSION 3.16)
project(06_robot_reference C)

set(FRAMEWORK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_executable(06_robot_reference
    main.c
    "${FRAMEWORK_ROOT}/products/bk7259_robot/main.c"
    "${FRAMEWORK_ROOT}/components/bk_core/src/bk_core.c"
    "${FRAMEWORK_ROOT}/components/bk_core/src/bk_event.c"
    "${FRAMEWORK_ROOT}/components/bk_board/src/bk_board.c"
    "${FRAMEWORK_ROOT}/components/bk_net/src/bk_net.c"
    "${FRAMEWORK_ROOT}/components/bk_net/src/bk_provision.c"
    "${FRAMEWORK_ROOT}/components/bk_net/src/bk_lan.c"
    "${FRAMEWORK_ROOT}/components/bk_media/src/bk_media.c"
    "${FRAMEWORK_ROOT}/components/bk_ui/src/bk_ui.c"
    "${FRAMEWORK_ROOT}/components/bk_peripheral/src/bk_peripheral.c"
    "${FRAMEWORK_ROOT}/components/bk_ota/src/bk_ota.c"
)

target_include_directories(06_robot_reference PRIVATE
    "${FRAMEWORK_ROOT}/products/bk7259_robot"
    "${FRAMEWORK_ROOT}/products/bk7259_robot/plugins"
    "${FRAMEWORK_ROOT}/components/bk_core/include"
    "${FRAMEWORK_ROOT}/components/bk_board/include"
    "${FRAMEWORK_ROOT}/components/bk_net/include"
    "${FRAMEWORK_ROOT}/components/bk_media/include"
    "${FRAMEWORK_ROOT}/components/bk_ui/include"
    "${FRAMEWORK_ROOT}/components/bk_peripheral/include"
    "${FRAMEWORK_ROOT}/components/bk_ota/include"
    "${FRAMEWORK_ROOT}/boards/bk7259_robot"
)

target_compile_features(06_robot_reference PRIVATE c_std_11)

enable_testing()
add_test(NAME 06_robot_reference_test COMMAND 06_robot_reference)
