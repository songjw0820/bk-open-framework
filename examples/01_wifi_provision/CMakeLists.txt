cmake_minimum_required(VERSION 3.16)
project(01_wifi_provision C)

add_executable(01_wifi_provision main.c)
target_compile_features(01_wifi_provision PRIVATE c_std_11)

# This host target checks the example entry point without requiring an SDK checkout.
