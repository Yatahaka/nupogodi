cmake_minimum_required(VERSION 3.16)
project(AppleCatcher VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)

add_executable(apple-catcher src/main.cpp)

target_include_directories(apple-catcher PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(apple-catcher PRIVATE ${SDL2_LIBRARIES})

if(UNIX AND NOT APPLE)
    target_link_libraries(apple-catcher PRIVATE m)
endif()
