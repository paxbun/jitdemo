# Copyright (c) 2022 Chanjung Kim. All rights reserved.

cmake_minimum_required(VERSION 3.13)
project(jitdemo)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(target_use_cpp_standard)

add_subdirectory(jitdemo.expr)
add_subdirectory(jitdemo.jit)
add_subdirectory(jitdemo.shell)
