cmake_minimum_required(VERSION 3.10)
project(hw2 C)

set(CMAKE_C_STANDARD 11)

# Add the runtime subdirectory
add_subdirectory(runtime)

add_executable(hw2 main.c
        bytefile.c
        interpreter.h
        interpreter.c)

# Link the runtime library to the executable
target_link_libraries(hw2 PRIVATE runtime)

# Include runtime headers
target_include_directories(hw2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/runtime)