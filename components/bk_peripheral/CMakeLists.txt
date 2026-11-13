cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(bk_peripheral C)
endif()

set(BK_PERIPHERAL_SOURCES src/bk_peripheral.c)
if(COMMAND armino_component_register)
    armino_component_register(SRCS ${BK_PERIPHERAL_SOURCES} INCLUDE_DIRS include
                              REQUIRES bk_core bk_board)
else()
    add_library(bk_peripheral STATIC ${BK_PERIPHERAL_SOURCES})
    target_include_directories(bk_peripheral PUBLIC include)
    target_include_directories(bk_peripheral PRIVATE ../bk_core/include ../bk_board/include)
    target_compile_features(bk_peripheral PUBLIC c_std_11)
endif()

option(BK_PERIPHERAL_BUILD_TESTS "Build bk_peripheral host tests" OFF)
if(BK_PERIPHERAL_BUILD_TESTS)
    enable_testing()
    add_executable(bk_peripheral_test tests/test_peripheral_caps.c
                   ${BK_PERIPHERAL_SOURCES} ../bk_board/src/bk_board.c)
    target_include_directories(bk_peripheral_test PRIVATE include ../bk_core/include
                               ../bk_board/include ../../boards/bk7259_robot)
    target_compile_features(bk_peripheral_test PRIVATE c_std_11)
    add_test(NAME bk_peripheral_test COMMAND bk_peripheral_test)
endif()
