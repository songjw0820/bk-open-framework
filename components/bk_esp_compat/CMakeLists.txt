if(COMMAND armino_component_register)
    set(srcs)
    if(CONFIG_BK_OF_ESP_COMPAT_ENABLE)
        list(APPEND srcs "src/bk_esp_compat.c")
    endif()
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "include"
        REQUIRES bk_common bk_port bk_core_event bk_storage bk_button bk_led_effect
    )
else()
    add_library(bk_esp_compat STATIC src/bk_esp_compat.c)
    target_include_directories(bk_esp_compat PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(bk_esp_compat PUBLIC
        bk_common bk_port bk_core_event bk_storage bk_button bk_led_effect
    )
endif()
