file(GLOB SOURCES "*.cpp" "*/*.cpp" "*/*/*.cpp")

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/third_party)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..)

add_executable(fast-chess ${SOURCES})

target_link_libraries(fast-chess)
