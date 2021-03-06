cmake_minimum_required(VERSION 3.5)
project(air)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
include_directories(src/common)
include_directories(src/tcp)
include_directories(src/ip)
include_directories(src/overlay)
include_directories(src/air)
include_directories(lib)

add_library(common_lib
            src/common/timer.cc
            src/common/seg.cc
            src/common/pkt.cc
            src/tcp/tcp_input.cc
            src/tcp/tcp_output.cc
            src/tcp/tcp_timer.cc
            src/tcp/tcp_send_buffer.cc
            src/tcp/tcp_recv_buffer.cc
            src/tcp/tcp.cc
            src/ip/ip.cc
            src/ip/ip_dvt.cc
            src/overlay/overlay.cc
            src/overlay/ov_ntable.cc
            src/air/air.cc)

add_executable(unit_test
               tests/unit_test.cc)
add_executable(con_client
               tests/con_client.cc)
add_executable(con_server
               tests/con_server.cc)
add_executable(file_client
               tests/file_client.cc)
add_executable(file_server
               tests/file_server.cc)
add_executable(echo_client
               tests/echo/echo_client.cc)
add_executable(echo_server
               tests/echo/echo_server.cc)
add_executable(loop
               tests/loop.cc)


target_compile_options(unit_test PRIVATE -Wall -Wuninitialized)
target_compile_options(file_client PRIVATE -Wall -Wuninitialized)
target_compile_options(file_server PRIVATE -Wall -Wuninitialized)
target_compile_options(loop PRIVATE -Wall -Wuninitialized)
target_compile_options(con_client PRIVATE -Wall -Wuninitialized)
target_compile_options(con_server PRIVATE -Wall -Wuninitialized)

target_link_libraries(unit_test common_lib Threads::Threads)
target_link_libraries(file_client common_lib Threads::Threads)
target_link_libraries(file_server common_lib Threads::Threads)
target_link_libraries(loop common_lib Threads::Threads)
target_link_libraries(con_client common_lib Threads::Threads)
target_link_libraries(con_server common_lib Threads::Threads)
