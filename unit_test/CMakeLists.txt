set(Wall_Flag "-Wall -Wno-sign-compare -Wno-format -Wno-reorder -Wno-unknown-pragmas")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -std=c++17 ${Wall_Flag} -g -O2")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -std=c++17 ${Wall_Flag} -g")

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin/unit_test)

set(MY_LIBS
    libboost_unit_test_framework.so
    libboost_test_exec_monitor.so
    bbt_core
    bbt_coroutine
    bbt_conet
    event_core
    event_pthreads
)


add_executable(Test_eventloop Test_eventloop.cc)
target_link_libraries(Test_eventloop ${MY_LIBS})
add_test(NAME Test_eventloop COMMAND Test_eventloop)

add_executable(Test_connection Test_connection.cc)
target_link_libraries(Test_connection ${MY_LIBS})
add_test(NAME Test_connection COMMAND Test_connection)