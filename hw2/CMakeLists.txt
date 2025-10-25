cmake_minimum_required(VERSION 3.10)
project(hw2 C)

set(CMAKE_C_STANDARD 11)

# Force 32-bit compilation
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -m32")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -m32")

# Add the runtime subdirectory
add_subdirectory(runtime)

add_executable(hw2 main.c
        bytefile.c
        interpreter.h
        interpreter.cpp)
#add_executable(byterun byterun.c)

# Link the runtime library to your executable
target_link_libraries(hw2 PRIVATE runtime)
#target_link_libraries(byterun PRIVATE runtime)

# Include runtime headers
target_include_directories(hw2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
#target_include_directories(byterun PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/runtime)