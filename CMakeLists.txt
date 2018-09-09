cmake_minimum_required(VERSION 3.0)

project(names.generator)

add_executable(generate main.cpp)

if (CYGWIN)
	add_definitions("--std=gnu++14 -D_WIN64")
endif ()
