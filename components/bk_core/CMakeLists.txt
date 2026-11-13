cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(bk_core C)
endif()

set(BK_CORE_SOURCES src/bk_core.c src/bk_event.c)

if(COMMAND armino_component_register)
    armino_component_register(SRCS ${BK_CORE_SOURCES} INCLUDE_DIRS include)
else()
    add_library(bk_core STATIC ${BK_CORE_SOURCES})
    target_include_directories(bk_core PUBLIC include)
    target_compile_features(bk_core PUBLIC c_std_11)
endif()

option(BK_CORE_BUILD_TESTS "Build bk_core host tests" OFF)
if(BK_CORE_BUILD_TESTS)
    enable_testing()
    add_executable(bk_core_test tests/test_event.c ${BK_CORE_SOURCES})
    target_include_directories(bk_core_test PRIVATE include)
    target_compile_features(bk_core_test PRIVATE c_std_11)
    add_test(NAME bk_core_test COMMAND bk_core_test)
endif()
