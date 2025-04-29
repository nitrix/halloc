cmake_minimum_required(VERSION 3.16)
project(halloc)

set(CMAKE_C_STANDARD 11)

add_library(halloc STATIC src/halloc.c)
target_include_directories(halloc PUBLIC include)
target_compile_options(halloc PRIVATE -Wall -Wextra -std=c11)
target_compile_options(halloc PRIVATE "$<$<CONFIG:DEBUG>:-O0;-g3;-ggdb3>")
target_compile_options(halloc PRIVATE "$<$<CONFIG:RELEASE>:-O3;-flto>")
target_link_options(halloc PRIVATE "$<$<CONFIG:RELEASE>:-flto>")

add_subdirectory(tests)