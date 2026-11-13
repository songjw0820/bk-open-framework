cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(bk7259_robot C)
endif()

set(BK7259_ROBOT_SOURCES main.c)
set(BK7259_ROBOT_STANDALONE OFF)

if(COMMAND armino_component_register)
    armino_component_register(
        SRCS ${BK7259_ROBOT_SOURCES}
        INCLUDE_DIRS . plugins
        REQUIRES bk_core bk_board bk_net)
else()
    set(BK7259_ROBOT_STANDALONE ON)
    add_library(bk7259_robot STATIC ${BK7259_ROBOT_SOURCES}
        ../../components/bk_core/src/bk_core.c
        ../../components/bk_core/src/bk_event.c
        ../../components/bk_board/src/bk_board.c
        ../../components/bk_net/src/bk_net.c
        ../../components/bk_net/src/bk_provision.c
        ../../components/bk_net/src/bk_lan.c)
    target_include_directories(bk7259_robot PUBLIC . plugins)
    target_include_directories(bk7259_robot PRIVATE
        ../../components/bk_core/include
        ../../components/bk_board/include
        ../../components/bk_net/include
        ../../boards/bk7259_robot)
    target_compile_features(bk7259_robot PUBLIC c_std_11)
endif()

option(BK7259_ROBOT_ENABLE_AGORA "Enable the explicitly injected Agora adapter" OFF)
if(BK7259_ROBOT_STANDALONE AND BK7259_ROBOT_ENABLE_AGORA)
    target_compile_definitions(bk7259_robot PRIVATE CONFIG_BK7259_ROBOT_AGORA=1)
endif()

option(BK7259_ROBOT_BUILD_TESTS "Build bk7259_robot host smoke tests" OFF)
if(BK7259_ROBOT_BUILD_TESTS)
    enable_testing()
    add_executable(bk7259_robot_test tests/test_product.c main.c
        ../../components/bk_core/src/bk_core.c
        ../../components/bk_core/src/bk_event.c
        ../../components/bk_board/src/bk_board.c
        ../../components/bk_net/src/bk_net.c
        ../../components/bk_net/src/bk_provision.c
        ../../components/bk_net/src/bk_lan.c)
    target_include_directories(bk7259_robot_test PRIVATE . plugins
        ../../components/bk_core/include
        ../../components/bk_board/include
        ../../components/bk_net/include
        ../../boards/bk7259_robot)
    target_compile_features(bk7259_robot_test PRIVATE c_std_11)
    target_compile_definitions(bk7259_robot_test PRIVATE BK7259_ROBOT_TEST_HOOKS=1)
    if(BK7259_ROBOT_ENABLE_AGORA)
        target_compile_definitions(bk7259_robot_test PRIVATE CONFIG_BK7259_ROBOT_AGORA=1)
    endif()
    add_test(NAME bk7259_robot_test COMMAND bk7259_robot_test)
endif()
