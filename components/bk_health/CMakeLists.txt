if(COMMAND armino_component_register)
    set(srcs)
    if(CONFIG_BK_OF_HEALTH_ENABLE)
        list(APPEND srcs "src/bk_health.c")
    endif()
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "include"
        REQUIRES bk_common bk_port
    )
else()
    add_library(bk_health STATIC src/bk_health.c)
    target_include_directories(bk_health PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(bk_health PUBLIC bk_common bk_port)
endif()
