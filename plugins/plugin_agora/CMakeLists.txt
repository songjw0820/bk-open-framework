if(COMMAND armino_component_register)
    set(srcs "src/bk_plugin_agora.c")
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "include"
        REQUIRES bk_common
    )
else()
    add_library(plugin_agora STATIC src/bk_plugin_agora.c)
    target_include_directories(plugin_agora PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(plugin_agora PUBLIC bk_common)
endif()
