# Copyright (c) 2022 Chanjung Kim. All rights reserved.

cmake_minimum_required(VERSION 3.13)

add_executable(jitdemo.shell
	"main.cxx"
)
target_link_libraries(jitdemo.shell
	jitdemo.expr
	jitdemo.jit
)
target_use_cpp_standard(jitdemo.shell 20)
