cmake_minimum_required(VERSION 3.28)
project(TEngine_ECS)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")


add_executable(TEngine_ECS main.cpp
                TEngine_ECS.hpp)

