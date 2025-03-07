cmake_minimum_required(VERSION 3.1)

project(bbtools-conet)

set(CMAKE_CXX_FALGS "${CMAKE_CXX_FLAGS} -std=c++17 -g")


set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib) 
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
include_directories(
    ${PROJECT_SOURCE_DIR}
    bbt    
)

file(GLOB_RECURSE SRCS 
    ${PROJECT_SOURCE_DIR}/bbt/*.cc
)

add_library(bbt_conet SHARED ${SRCS})

target_link_libraries(bbt_conet
    bbt_core
    bbt_coroutine
)

enable_testing()
add_subdirectory(unit_test)

add_subdirectory(example)
