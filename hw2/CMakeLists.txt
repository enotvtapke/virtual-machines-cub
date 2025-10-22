cmake_minimum_required(VERSION 4.0)
project(hw2)

set(CMAKE_CXX_STANDARD 20)

add_executable(hw2 main.cpp)

target_compile_options(hw2 PRIVATE
        -m32
)