if(COMMAND armino_component_register)
    set(srcs)
    if(CONFIG_BK_OF_AUDIO_MIXER_ENABLE)
        list(APPEND srcs "src/bk_audio_mixer.c")
    endif()
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "include"
        REQUIRES bk_common bk_audio_session
    )
else()
    add_library(bk_audio_mixer STATIC src/bk_audio_mixer.c)
    target_include_directories(bk_audio_mixer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(bk_audio_mixer PUBLIC bk_common bk_audio_session)
endif()
