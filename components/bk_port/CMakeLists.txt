if(COMMAND armino_component_register)
    set(srcs)
    if(CONFIG_BK_OF_PORT_ENABLE)
        list(APPEND srcs "bk7259_v4/bk_port_bk7259.c")
    endif()
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "include"
        REQUIRES bk_common
    )
else()
    add_library(bk_port STATIC posix_sim/bk_port_posix.c)
    target_include_directories(bk_port PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(bk_port PUBLIC bk_common pthread)
endif()
