cmake_minimum_required(VERSION 3.0)

project(fast-chess)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -Wall -Wextra -DNDEBUG")

set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src)

file(GLOB SRC_FILES ${CMAKE_SOURCE_DIR}/src/*.cpp ${CMAKE_SOURCE_DIR}/src/*/*.cpp ${CMAKE_SOURCE_DIR}/src/*/*/*.cpp)

add_definitions(-DNDEBUG)

set(TARGET fast-chess)
add_executable(${TARGET} ${SRC_FILES})

file(GLOB TEST_SRC ${CMAKE_SOURCE_DIR}/tests/*.cpp)
list(REMOVE_ITEM SRC_FILES ${CMAKE_SOURCE_DIR}/src/main.cpp)  # Exclude main.cpp

set(TEST_TARGET fast-chess-tests)
add_executable(${TEST_TARGET} ${SRC_FILES} ${TEST_SRC})

include_directories(${INCLUDE_DIR})

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(Threads REQUIRED)
    target_link_libraries(${TARGET} Threads::Threads)
    target_link_libraries(${TEST_TARGET} Threads::Threads)
endif()
