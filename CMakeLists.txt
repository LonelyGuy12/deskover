cmake_minimum_required(VERSION 3.20)
project(WinUpOverlay CXX)

# Enforce x64 compiling and C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "This project requires a 64-bit build (x64).")
endif()

# We prefer static CRT for standalone distribution where possible
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_subdirectory(OverlayApp)
