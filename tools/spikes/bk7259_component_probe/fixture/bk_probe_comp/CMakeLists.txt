set(incs include)
set(srcs)

if(CONFIG_BK_OPEN_FRAMEWORK_PROBE)
    list(APPEND srcs src/bk_probe_comp.c)
endif()

armino_component_register(SRCS "${srcs}" INCLUDE_DIRS "${incs}")

if(CONFIG_BK_OPEN_FRAMEWORK_PROBE)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,-u,bk_open_framework_probe_marker")
endif()
