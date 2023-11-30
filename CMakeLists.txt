cmake_minimum_required(VERSION 3.12)
project(fast-chess-project)

set(CMAKE_CXX_STANDARD 17)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(src)
add_subdirectory(tests)