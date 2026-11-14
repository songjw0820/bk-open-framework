if(COMMAND armino_component_register)
    set(srcs)
    if(CONFIG_BK_OF_PRODUCT_MOTION_ENABLE)
        list(APPEND srcs "src/bk_motion.c")
    endif()
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "include"
        REQUIRES bk_common bk_board
    )
else()
    add_library(bk_motion STATIC src/bk_motion.c)
    target_include_directories(bk_motion PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(bk_motion PUBLIC bk_common bk_board)
endif()
