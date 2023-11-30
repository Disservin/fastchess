file(GLOB TEST_SOURCES "*.cpp")
file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_LIST_DIR}/../src/*.cpp")

list(REMOVE_ITEM SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/main.cpp)

include_directories(${CMAKE_CURRENT_LIST_DIR}/../src ${CMAKE_CURRENT_LIST_DIR}/../third_party)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..)

add_executable(fast-chess-tests ${TEST_SOURCES} ${SOURCES})

target_link_libraries(fast-chess-tests)