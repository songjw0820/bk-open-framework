if(COMMAND armino_component_register)
    set(srcs)
    if(CONFIG_BK_OF_VIDEO_FRAME_POOL_ENABLE)
        list(APPEND srcs "src/bk_video_frame_pool.c")
    endif()
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "include"
        REQUIRES bk_common bk_port
    )
else()
    add_library(bk_video_frame_pool STATIC src/bk_video_frame_pool.c)
    target_include_directories(bk_video_frame_pool PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(bk_video_frame_pool PUBLIC bk_common bk_port)
endif()
