if(COMMAND armino_component_register)
    set(srcs)
    if(CONFIG_BK_OF_STORAGE_ENABLE)
        list(APPEND srcs "src/bk_storage.c")
    endif()
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "include"
        REQUIRES bk_common bk_port
    )
else()
    add_library(bk_storage STATIC src/bk_storage.c)
    target_include_directories(bk_storage PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(bk_storage PUBLIC bk_common bk_port)
endif()
