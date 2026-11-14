if(COMMAND armino_component_register)
    set(srcs)
    if(CONFIG_BK_OF_AI_AGENT_ENABLE)
        list(APPEND srcs
            "transport_websocket/src/bk_ws_transport.c"
            "protocol_xiaozhi/src/bk_protocol_xiaozhi.c"
        )
    endif()
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS
            "transport_websocket/include"
            "protocol_xiaozhi/include"
        REQUIRES bk_common bk_port bk_net bk_audio_session
        PRIV_REQUIRES bk_libs psa_mbedtls lwip_intf_v2_1
    )
    if(CONFIG_BK_OF_AI_AGENT_ENABLE)
        target_compile_definitions(${COMPONENT_LIB} PRIVATE BK_OF_BUILDING_TARGET_BK7259=1)
    endif()
else()
    add_library(bk_ai_agent STATIC
        transport_websocket/src/bk_ws_transport.c
        protocol_xiaozhi/src/bk_protocol_xiaozhi.c
    )
    target_include_directories(bk_ai_agent PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/transport_websocket/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/protocol_xiaozhi/include"
    )
    target_link_libraries(bk_ai_agent PUBLIC bk_common bk_port bk_net bk_audio_session ${CMAKE_DL_LIBS})
endif()
