set(Wall_Flag "-Wall -Wno-sign-compare -Wno-format -Wno-reorder -Wno-unknown-pragmas")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -std=c++17 ${Wall_Flag} -g")

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin/example)

set(MY_LIBS
    bbt_core
    bbt_coroutine
    bbt_conet
    event_core
    event_pthreads
)

add_executable(echo_client echo/echo_client.cc)
target_link_libraries(echo_client ${MY_LIBS})

add_executable(echo_server echo/echo_server.cc)
target_link_libraries(echo_server ${MY_LIBS})
