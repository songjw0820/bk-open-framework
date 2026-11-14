if(COMMAND armino_component_register)
    set(srcs)
    if(CONFIG_BK_OF_BOARD_ENABLE)
        list(APPEND srcs "src/bk_board.c")
    endif()
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "include"
        REQUIRES bk_common
    )
else()
    add_library(bk_board STATIC src/bk_board.c)
    target_include_directories(bk_board PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(bk_board PUBLIC bk_common)
endif()
