if(COMMAND armino_component_register)
    set(srcs)
    if(CONFIG_BK_OF_LED_EFFECT_ENABLE)
        list(APPEND srcs "src/bk_led_effect.c")
    endif()
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "include"
        REQUIRES bk_common bk_port
    )
else()
    add_library(bk_led_effect STATIC src/bk_led_effect.c)
    target_include_directories(bk_led_effect PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(bk_led_effect PUBLIC bk_common bk_port)
endif()
