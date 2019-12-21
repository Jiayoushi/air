cmake_minimum_required(VERSION 3.5)
project(air)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
include_directories(src)

add_library(common_lib
            src/common/seg.cc
            src/common/timer.cc
            src/common/send_buffer.cc
            src/client/srt_client.cc
            src/server/srt_server.cc
            src/client/srt_client_overlay.cc
            src/server/srt_server_overlay.cc)

add_executable(air_client
               src/client/app_client.cc)
add_executable(air_server
               src/server/app_server.cc)
add_executable(unit_test
               tests/unit_test.cc)
add_executable(file_client
               tests/file_client.cc)
add_executable(file_server
               tests/file_server.cc)

target_compile_options(air_client PRIVATE -Wall)
target_compile_options(air_server PRIVATE -Wall)
target_compile_options(unit_test PRIVATE -Wall)
target_compile_options(file_client PRIVATE -Wall)
target_compile_options(file_server PRIVATE -Wall)

target_link_libraries(air_client common_lib Threads::Threads)
target_link_libraries(air_server common_lib Threads::Threads)
target_link_libraries(unit_test common_lib Threads::Threads)
target_link_libraries(file_client common_lib Threads::Threads)
target_link_libraries(file_server common_lib Threads::Threads)
