if(NOT DEFINED BK_OPEN_FRAMEWORK_ROOT)
    get_filename_component(BK_OPEN_FRAMEWORK_ROOT "${CMAKE_CURRENT_LIST_FILE}/../../../.." ABSOLUTE)
endif()

set(bk_open_framework_common_srcs
    "${BK_OPEN_FRAMEWORK_ROOT}/components/bk_core/src/bk_core.c"
    "${BK_OPEN_FRAMEWORK_ROOT}/components/bk_core/src/bk_event.c"
    "${BK_OPEN_FRAMEWORK_ROOT}/components/bk_board/src/bk_board.c"
    "${BK_OPEN_FRAMEWORK_ROOT}/components/bk_peripheral/src/bk_peripheral.c"
)

set(bk_open_framework_ap_srcs
    "${BK_OPEN_FRAMEWORK_ROOT}/components/bk_net/src/bk_net.c"
    "${BK_OPEN_FRAMEWORK_ROOT}/components/bk_net/src/bk_provision.c"
    "${BK_OPEN_FRAMEWORK_ROOT}/components/bk_net/src/bk_lan.c"
    "${BK_OPEN_FRAMEWORK_ROOT}/components/bk_ota/src/bk_ota.c"
    "${BK_OPEN_FRAMEWORK_ROOT}/components/bk_media/src/bk_media.c"
    "${BK_OPEN_FRAMEWORK_ROOT}/components/bk_ui/src/bk_ui.c"
    "${BK_OPEN_FRAMEWORK_ROOT}/products/bk7259_robot/main.c"
)

if(ARMINO_SOC STREQUAL "bk7259")
    set(bk_open_framework_srcs ${bk_open_framework_common_srcs})
else()
    set(bk_open_framework_srcs
        ${bk_open_framework_common_srcs}
        ${bk_open_framework_ap_srcs}
    )
endif()

set(bk_open_framework_incs
    "${BK_OPEN_FRAMEWORK_ROOT}/components/bk_core/include"
    "${BK_OPEN_FRAMEWORK_ROOT}/components/bk_board/include"
    "${BK_OPEN_FRAMEWORK_ROOT}/components/bk_net/include"
    "${BK_OPEN_FRAMEWORK_ROOT}/components/bk_ota/include"
    "${BK_OPEN_FRAMEWORK_ROOT}/components/bk_peripheral/include"
    "${BK_OPEN_FRAMEWORK_ROOT}/components/bk_media/include"
    "${BK_OPEN_FRAMEWORK_ROOT}/components/bk_ui/include"
    "${BK_OPEN_FRAMEWORK_ROOT}/boards/bk7259_robot"
    "${BK_OPEN_FRAMEWORK_ROOT}/products/bk7259_robot"
    "${BK_OPEN_FRAMEWORK_ROOT}/products/bk7259_robot/plugins"
)

armino_component_register(
    SRCS "${bk_open_framework_srcs}"
    INCLUDE_DIRS "${bk_open_framework_incs}"
)

if(ARMINO_SOC STREQUAL "bk7259_ap")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE BK_ARMINO_SUBSYSTEM_AP=1)
    target_link_libraries(${COMPONENT_LIB} INTERFACE
        "-Wl,-u,bk7259_robot_start"
        "-Wl,-u,bk_net_default_config"
        "-Wl,-u,bk_provision_default_config"
        "-Wl,-u,bk_lan_default_config"
        "-Wl,-u,bk_ota_default_adapter"
        "-Wl,-u,bk_peripheral_default_config"
        "-Wl,-u,bk_media_default_adapter"
        "-Wl,-u,bk_ui_default_adapter"
    )
elseif(ARMINO_SOC STREQUAL "bk7259")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE BK_ARMINO_SUBSYSTEM_CP=1)
    target_link_libraries(${COMPONENT_LIB} INTERFACE
        "-Wl,-u,bk_peripheral_default_config"
    )
endif()

