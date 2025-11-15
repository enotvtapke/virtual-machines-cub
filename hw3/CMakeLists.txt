cmake_minimum_required(VERSION 3.10)
project(hw3 CXX)

set(CMAKE_CXX_STANDARD 23)

add_executable(hw3 main.cpp
        bytefile.cpp
        analizer.h
        analizer.cpp
        shared.cpp
        shared.h)
