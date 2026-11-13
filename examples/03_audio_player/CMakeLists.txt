cmake_minimum_required(VERSION 3.16)
project(03_audio_player C)

add_executable(03_audio_player main.c)
target_compile_features(03_audio_player PRIVATE c_std_11)

# This host target checks the example entry point without requiring an SDK checkout.
