if(COMMAND armino_component_register)
    set(srcs)
    if(CONFIG_BK_OF_PRODUCT_BATTERY_POLICY_ENABLE)
        list(APPEND srcs "src/bk_battery_policy.c")
    endif()
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "include"
        REQUIRES bk_common bk_board
    )
else()
    add_library(bk_battery_policy STATIC src/bk_battery_policy.c)
    target_include_directories(bk_battery_policy PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(bk_battery_policy PUBLIC bk_common bk_board)
endif()
