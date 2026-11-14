if(COMMAND armino_component_register)
    set(srcs)
    if(CONFIG_BK_OF_AUDIO_SESSION_ENABLE)
        list(APPEND srcs "src/bk_audio_session.c")
    endif()
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "include"
        REQUIRES bk_common bk_port bk_board
    )
else()
    add_library(bk_audio_session STATIC src/bk_audio_session.c)
    target_include_directories(bk_audio_session PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(bk_audio_session PUBLIC bk_common bk_port bk_board)
endif()
