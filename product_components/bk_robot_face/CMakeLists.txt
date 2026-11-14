if(COMMAND armino_component_register)
    set(srcs)
    if(CONFIG_BK_OF_PRODUCT_ROBOT_FACE_ENABLE)
        list(APPEND srcs "src/bk_robot_face.c")
    endif()
    armino_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "include"
        REQUIRES bk_common bk_ui_core
    )
else()
    add_library(bk_robot_face STATIC src/bk_robot_face.c)
    target_include_directories(bk_robot_face PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(bk_robot_face PUBLIC bk_common bk_ui_core)
endif()
