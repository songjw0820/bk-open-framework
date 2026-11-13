cmake_minimum_required(VERSION 3.16)
project(08_ota_update C)

set(FRAMEWORK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")

add_executable(08_ota_update
    main.c
    "${FRAMEWORK_ROOT}/components/bk_core/src/bk_core.c"
    "${FRAMEWORK_ROOT}/components/bk_core/src/bk_event.c"
    "${FRAMEWORK_ROOT}/components/bk_board/src/bk_board.c"
    "${FRAMEWORK_ROOT}/components/bk_net/src/bk_net.c"
    "${FRAMEWORK_ROOT}/components/bk_net/src/bk_provision.c"
    "${FRAMEWORK_ROOT}/components/bk_net/src/bk_lan.c"
    "${FRAMEWORK_ROOT}/components/bk_ota/src/bk_ota.c"
)

target_include_directories(08_ota_update PRIVATE
    "${FRAMEWORK_ROOT}/components/bk_core/include"
    "${FRAMEWORK_ROOT}/components/bk_board/include"
    "${FRAMEWORK_ROOT}/components/bk_net/include"
    "${FRAMEWORK_ROOT}/components/bk_ota/include"
    "${FRAMEWORK_ROOT}/boards/bk7259_robot"
)

target_compile_features(08_ota_update PRIVATE c_std_11)

enable_testing()
add_test(NAME 08_ota_update_test COMMAND 08_ota_update)
