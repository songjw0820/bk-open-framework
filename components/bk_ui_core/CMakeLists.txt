if(COMMAND armino_component_register)
    set(srcs)
    if(CONFIG_BK_OF_UI_CORE_ENABLE)
        list(APPEND srcs "src/bk_ui_core.c")
    endif()
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "include"
        REQUIRES bk_common bk_port bk_board
    )
else()
    add_library(bk_ui_core STATIC src/bk_ui_core.c)
    target_include_directories(bk_ui_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(bk_ui_core PUBLIC bk_common bk_port bk_board)
endif()
