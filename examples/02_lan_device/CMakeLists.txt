cmake_minimum_required(VERSION 3.16)
project(02_lan_device C)

add_executable(02_lan_device main.c)
target_compile_features(02_lan_device PRIVATE c_std_11)

# This host target checks the example entry point without requiring an SDK checkout.
