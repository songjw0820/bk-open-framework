cmake_minimum_required(VERSION 3.16)
project(04_camera_stream C)

add_executable(04_camera_stream main.c)
target_compile_features(04_camera_stream PRIVATE c_std_11)

# This host target checks the example entry point without requiring an SDK checkout.
