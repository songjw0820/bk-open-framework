if(COMMAND armino_component_register)
    set(srcs)
    if(CONFIG_BK_OF_OTA_ENABLE)
        list(APPEND srcs "src/bk_ota.c")
    endif()
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "include"
        REQUIRES bk_common bk_port bk_board bk_storage
    )
else()
    add_library(bk_ota STATIC src/bk_ota.c)
    target_include_directories(bk_ota PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(bk_ota PUBLIC bk_common bk_port bk_board bk_storage)
endif()
