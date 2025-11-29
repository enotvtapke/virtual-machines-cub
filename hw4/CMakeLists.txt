cmake_minimum_required(VERSION 3.10)
project(hw4 C CXX)

set(CMAKE_CXX_STANDARD 23)

# Add the runtime subdirectory (C library)
add_subdirectory(runtime)

set_target_properties(runtime PROPERTIES
        LINKER_LANGUAGE C
)

add_executable(hw4 main.cpp
        bytefile.cpp
        analizer.h
        analizer.cpp
        shared.cpp
        shared.h
        interpreter.cpp
        interpreter.h)

# Link the runtime library to hw4
target_link_libraries(hw4 PRIVATE runtime)

# Add include directory so you can #include runtime headers
target_include_directories(hw4 PRIVATE ${CMAKE_SOURCE_DIR}/runtime)