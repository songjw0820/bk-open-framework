cmake_minimum_required(VERSION 3.16)
project(05_peripheral_control C)

add_executable(05_peripheral_control main.c)
target_compile_features(05_peripheral_control PRIVATE c_std_11)

# This host target checks the example entry point without requiring an SDK checkout.
