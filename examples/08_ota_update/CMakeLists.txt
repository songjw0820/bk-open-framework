cmake_minimum_required(VERSION 3.16)
project(08_ota_update C)

add_executable(08_ota_update main.c)
target_compile_features(08_ota_update PRIVATE c_std_11)

# This host target checks the example entry point without requiring an SDK checkout.
