cmake_minimum_required(VERSION 3.5)
project(air)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
include_directories(lib)
add_executable(air_client
			   src/client/app_client.cc
               src/client/srt_client.cc
               src/common/seg.cc
               src/common/timer.cc)
add_executable(air_server
               src/server/app_server.cc
               src/server/srt_server.cc
               src/common/seg.cc
               src/common/timer.cc)
target_compile_options(air_client PRIVATE -Wall)
target_compile_options(air_server PRIVATE -Wall)
target_link_libraries(air_client Threads::Threads)
target_link_libraries(air_server Threads::Threads)
