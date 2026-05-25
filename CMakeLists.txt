cmake_minimum_required(VERSION 3.16)
project(AppleCatcher VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(apple-catcher src/main.cpp)

if(EMSCRIPTEN)
    set_target_properties(apple-catcher PROPERTIES SUFFIX ".html")

    target_compile_options(apple-catcher PRIVATE
        -sUSE_SDL=2
    )
    target_link_options(apple-catcher PRIVATE
        -sUSE_SDL=2
        -sALLOW_MEMORY_GROWTH=1
        -sENVIRONMENT=web
    )
else()
    find_package(SDL2 REQUIRED)
    target_include_directories(apple-catcher PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(apple-catcher PRIVATE ${SDL2_LIBRARIES})
    if(UNIX AND NOT APPLE)
        target_link_libraries(apple-catcher PRIVATE m)
    endif()
endif()
