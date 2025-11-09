cmake_minimum_required(VERSION 3.10)
project(hw3 C)

set(CMAKE_C_STANDARD 11)

# Add the runtime subdirectory
add_subdirectory(runtime)

add_executable(hw3 main.c
        bytefile.c
        interpreter.h
        interpreter.c)

# Link the runtime library to the executable
target_link_libraries(hw3 PRIVATE runtime)

# Include runtime headers
target_include_directories(hw3 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/runtime)