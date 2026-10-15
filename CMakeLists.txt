cmake_minimum_required(VERSION 3.20)
project(gui_dx11 CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)

add_subdirectory(wm)
add_subdirectory(Renderer)
add_subdirectory(GUI)
add_subdirectory(Cloud)
add_subdirectory(demo)
