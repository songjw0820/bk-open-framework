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
add_subdirectory(components/bk_storage)
add_subdirectory(components/bk_health)
add_subdirectory(components/bk_board)
add_subdirectory(components/bk_net)
add_subdirectory(components/bk_button)
add_subdirectory(components/bk_led_effect)
add_subdirectory(components/bk_audio_session)
add_subdirectory(components/bk_audio_mixer)
add_subdirectory(components/bk_video_frame_pool)
add_subdirectory(components/bk_ui_core)
add_subdirectory(components/bk_ai_agent)
add_subdirectory(components/bk_ota)
add_subdirectory(components/bk_esp_compat)

add_subdirectory(product_components/bk_motion)
add_subdirectory(product_components/bk_robot_face)
add_subdirectory(product_components/bk_battery_policy)
add_subdirectory(plugins/plugin_agora)

add_library(bk_robot_product STATIC products/bk7259_robot/src/bk_robot_product.c)
target_include_directories(bk_robot_product PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/products/bk7259_robot/include")
target_link_libraries(bk_robot_product PUBLIC
    bk_ai_agent bk_motion bk_robot_face bk_battery_policy
    bk_ui_core bk_audio_session bk_board bk_port bk_common
)

add_executable(test_event_loop components/bk_core_event/tests/test_event_loop.c)
target_link_libraries(test_event_loop PRIVATE bk_core_event bk_port bk_common)
add_test(NAME test_event_loop COMMAND test_event_loop)

add_executable(test_storage_security components/bk_storage/tests/test_storage_security.c)
target_link_libraries(test_storage_security PRIVATE bk_storage bk_port bk_common)
add_test(NAME test_storage_security COMMAND test_storage_security)

add_executable(test_board_caps components/bk_board/tests/test_board_caps.c)
target_link_libraries(test_board_caps PRIVATE bk_board bk_health bk_port bk_common)
add_test(NAME test_board_caps COMMAND test_board_caps)

add_executable(test_net_fsm components/bk_net/tests/test_net_fsm.c)
target_link_libraries(test_net_fsm PRIVATE bk_net bk_storage bk_core_event bk_port bk_common)
add_test(NAME test_net_fsm COMMAND test_net_fsm)

add_executable(example_01_wifi_provision examples/01_wifi_provision/main.c)
target_link_libraries(example_01_wifi_provision PRIVATE bk_net bk_board bk_storage bk_core_event bk_port bk_common)
add_test(NAME example_01_wifi_provision COMMAND example_01_wifi_provision)

add_executable(example_02_lan_device examples/02_lan_device/main.c)
target_link_libraries(example_02_lan_device PRIVATE bk_net bk_board bk_port bk_common)
add_test(NAME example_02_lan_device COMMAND example_02_lan_device)

add_executable(test_peripherals_and_media components/bk_button/tests/test_peripherals_and_media.c)
target_link_libraries(test_peripherals_and_media PRIVATE
    bk_button bk_led_effect bk_audio_mixer bk_audio_session
    bk_video_frame_pool bk_ui_core bk_board bk_port bk_common
)
add_test(NAME test_peripherals_and_media COMMAND test_peripherals_and_media)

add_executable(example_03_audio_player examples/03_audio_player/main.c)
target_link_libraries(example_03_audio_player PRIVATE bk_audio_mixer bk_audio_session bk_board bk_port bk_common)
add_test(NAME example_03_audio_player COMMAND example_03_audio_player)

add_executable(example_04_camera_stream examples/04_camera_stream/main.c)
target_link_libraries(example_04_camera_stream PRIVATE bk_video_frame_pool bk_ui_core bk_board bk_port bk_common)
add_test(NAME example_04_camera_stream COMMAND example_04_camera_stream)

add_executable(example_05_peripheral_control examples/05_peripheral_control/main.c)
target_link_libraries(example_05_peripheral_control PRIVATE bk_button bk_led_effect bk_board bk_port bk_common)
add_test(NAME example_05_peripheral_control COMMAND example_05_peripheral_control)

add_executable(test_protocol_xiaozhi components/bk_ai_agent/tests/test_protocol_xiaozhi.c)
target_link_libraries(test_protocol_xiaozhi PRIVATE bk_ai_agent bk_audio_session bk_net bk_board bk_port bk_common)
add_test(NAME test_protocol_xiaozhi COMMAND test_protocol_xiaozhi)

add_executable(example_06_robot_reference examples/06_robot_reference/main.c)
target_link_libraries(example_06_robot_reference PRIVATE
    bk_robot_product bk_ai_agent bk_motion bk_robot_face bk_battery_policy plugin_agora
    bk_ui_core bk_audio_session bk_net bk_storage bk_core_event bk_board bk_port bk_common
)
add_test(NAME example_06_robot_reference COMMAND example_06_robot_reference)

add_executable(test_ota_fsm components/bk_ota/tests/test_ota_fsm.c)
target_link_libraries(test_ota_fsm PRIVATE bk_ota bk_storage bk_board bk_port bk_common)
add_test(NAME test_ota_fsm COMMAND test_ota_fsm)

add_executable(test_esp_compat components/bk_esp_compat/tests/test_esp_compat.c)
target_link_libraries(test_esp_compat PRIVATE bk_esp_compat bk_port bk_common)
add_test(NAME test_esp_compat COMMAND test_esp_compat)

# Compile & verify the BK7259 target code paths (bk_port_bk7259.c + mbedTLS wss:// + <modules/opus.h>)
add_executable(test_bk7259_target_adapter
    tests/test_bk7259_target_adapter.c
    components/bk_port/bk7259_v4/bk_port_bk7259.c
    components/bk_ai_agent/transport_websocket/src/bk_ws_transport.c
    components/bk_ai_agent/protocol_xiaozhi/src/bk_protocol_xiaozhi.c
    components/bk_audio_session/src/bk_audio_session.c
    components/bk_board/src/bk_board.c
    components/bk_net/src/bk_net.c
    components/bk_storage/src/bk_storage.c
    components/bk_core_event/src/bk_core_event.c
)
target_compile_definitions(test_bk7259_target_adapter PRIVATE BK_OF_BUILDING_TARGET_BK7259=1)
target_include_directories(test_bk7259_target_adapter PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/armino_sdk_stub/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/bk_port/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/bk_ai_agent/transport_websocket/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/bk_ai_agent/protocol_xiaozhi/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/bk_audio_session/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/bk_board/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/bk_net/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/bk_storage/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/bk_core_event/include"
)
target_link_libraries(test_bk7259_target_adapter PRIVATE bk_common pthread)
add_test(NAME test_bk7259_target_adapter COMMAND test_bk7259_target_adapter)
