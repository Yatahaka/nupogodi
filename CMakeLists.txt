cmake_minimum_required(VERSION 3.16)
project(apple_catcher)

set(CMAKE_CXX_STANDARD 17)

add_executable(apple_catcher src/main.cpp)

if(EMSCRIPTEN)
    set_target_properties(apple_catcher PROPERTIES SUFFIX ".html")
    target_compile_options(apple_catcher PRIVATE -sUSE_SDL=3)
    target_link_options(apple_catcher PRIVATE
        -sUSE_SDL=3
        -sALLOW_MEMORY_GROWTH=1
        -sENVIRONMENT=web
    )
else()
    find_package(SDL3 REQUIRED)
    target_link_libraries(apple_catcher SDL3::SDL3)
endif()
