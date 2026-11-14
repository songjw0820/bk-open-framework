if(COMMAND armino_component_register)
    set(srcs)
    if(CONFIG_BK_OF_CORE_EVENT_ENABLE)
        list(APPEND srcs "src/bk_core_event.c")
    endif()
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "include"
        REQUIRES bk_common bk_port
    )
else()
    add_library(bk_core_event STATIC src/bk_core_event.c)
    target_include_directories(bk_core_event PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(bk_core_event PUBLIC bk_common bk_port)
endif()
