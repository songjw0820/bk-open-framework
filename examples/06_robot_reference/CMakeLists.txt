cmake_minimum_required(VERSION 3.16)
project(06_robot_reference C)

add_executable(06_robot_reference main.c)
target_compile_features(06_robot_reference PRIVATE c_std_11)

# This host target checks the example entry point without requiring an SDK checkout.
