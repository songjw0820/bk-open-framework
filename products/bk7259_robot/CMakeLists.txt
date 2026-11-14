if(COMMAND armino_component_register)
    set(srcs)
    if(CONFIG_BK_OF_PRODUCT_ROBOT_ENABLE)
        list(APPEND srcs "src/bk_robot_product.c")
    endif()
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "include"
        REQUIRES
            bk_common bk_port bk_board bk_ui_core bk_audio_session
            bk_ai_agent bk_motion bk_robot_face bk_battery_policy
    )
else()
    add_library(bk_robot_product STATIC src/bk_robot_product.c)
    target_include_directories(bk_robot_product PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(bk_robot_product PUBLIC
        bk_ai_agent bk_motion bk_robot_face bk_battery_policy
        bk_ui_core bk_audio_session bk_board bk_port bk_common
    )
endif()
