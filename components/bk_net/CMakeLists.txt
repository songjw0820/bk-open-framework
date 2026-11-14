if(COMMAND armino_component_register)
    set(srcs)
    if(CONFIG_BK_OF_NET_ENABLE)
        list(APPEND srcs
            "src/bk_net.c"
            "src/bk_provision.c"
            "src/bk_lan.c"
        )
    endif()
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "include"
        REQUIRES bk_common bk_port bk_core_event bk_storage
    )
else()
    add_library(bk_net STATIC
        src/bk_net.c
        src/bk_provision.c
        src/bk_lan.c
    )
    target_include_directories(bk_net PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(bk_net PUBLIC bk_common bk_port bk_core_event bk_storage)
endif()
