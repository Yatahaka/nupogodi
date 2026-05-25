cmake_minimum_required(VERSION 3.16)
project(apple_catcher)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.2.0
    GIT_SHALLOW    TRUE
)

set(SDL_SHARED          OFF CACHE BOOL "" FORCE)
set(SDL_STATIC          ON  CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY    OFF CACHE BOOL "" FORCE)
set(SDL_TESTS           OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(SDL3)

add_executable(apple_catcher src/main.cpp)
target_link_libraries(apple_catcher PRIVATE SDL3::SDL3-static)

if(EMSCRIPTEN)
    set_target_properties(apple_catcher PROPERTIES SUFFIX ".html")
    target_link_options(apple_catcher PRIVATE
        -sALLOW_MEMORY_GROWTH=1
        -sENVIRONMENT=web
    )
endif()
