cmake_minimum_required(VERSION 3.10)
project(SprintLayoutFilter C)

# Sprint-Layout 6.0 is 32-bit (x86)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(WARNING "WARNING: Sprint-Layout is a 32-bit process. Please configure CMake for 32-bit x86 (e.g. cmake -A Win32 ..)")
endif()

set(CMAKE_C_STANDARD 99)

add_library(version SHARED
    src/slfilter.c
    src/version.def
    src/version.rc
)

target_link_libraries(version PRIVATE user32 kernel32 gdi32)

set_target_properties(version PROPERTIES
    PREFIX ""
    OUTPUT_NAME "version"
)
