# Copyright (c) 2022 Chanjung Kim. All rights reserved.

cmake_minimum_required(VERSION 3.13)

add_library(jitdemo.jit
	"compiled-function-allocator.ixx"
	"compiled-function.ixx"
	"mod.ixx"
)
target_link_libraries(jitdemo.jit
	jitdemo.expr
)
target_use_cpp_standard(jitdemo.jit 20)
