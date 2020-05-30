cmake_minimum_required(VERSION 3.10)
project(os_midterm)

set(CMAKE_CXX_STANDARD 11)

add_executable(fileSystemOper file_system_oper.cpp args_reader.cpp args_reader.h file_system.cpp file_system.h)
add_executable(makeFileSystem make_file_system.cpp args_reader.cpp args_reader.h file_system.cpp file_system.h)