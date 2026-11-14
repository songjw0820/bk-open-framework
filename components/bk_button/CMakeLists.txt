if(COMMAND armino_component_register)
    set(srcs)
    if(CONFIG_BK_OF_BUTTON_ENABLE)
        list(APPEND srcs "src/bk_button.c")
    endif()
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "include"
        REQUIRES bk_common bk_port
    )
else()
    add_library(bk_button STATIC src/bk_button.c)
    target_include_directories(bk_button PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(bk_button PUBLIC bk_common bk_port)
endif()
