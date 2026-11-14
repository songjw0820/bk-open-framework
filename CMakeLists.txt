cmake_minimum_required(VERSION 3.20)
project(bk_open_framework C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -Werror)

option(BK_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer on host" ON)
if(BK_ENABLE_SANITIZERS AND NOT CMAKE_CROSSCOMPILING)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

enable_testing()

add_subdirectory(components/bk_common)
add_subdirectory(components/bk_port)
add_subdirectory(components/bk_core_event)

add_executable(test_event_loop components/bk_core_event/tests/test_event_loop.c)
target_link_libraries(test_event_loop PRIVATE bk_core_event bk_port bk_common)
add_test(NAME test_event_loop COMMAND test_event_loop)
