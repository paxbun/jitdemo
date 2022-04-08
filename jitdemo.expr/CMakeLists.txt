# Copyright (c) 2022 Chanjung Kim. All rights reserved.

cmake_minimum_required(VERSION 3.13)

add_library(jitdemo.expr
	"expression.ixx"

	"expressions/binary-expression.ixx" 
	"expressions/constant-expression.ixx"
	"expressions/variable-expression.ixx"
	"expressions/mod.ixx"

	"mod.ixx"
)
target_use_cpp_standard(jitdemo.expr 20)
