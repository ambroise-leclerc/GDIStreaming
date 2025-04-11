cmake_minimum_required(VERSION 3.10)
project(GrayscaleNoiseApp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
add_executable(client WIN32 client.cpp)

# Link necessary Windows libraries
if(WIN32)
    target_link_libraries(client PRIVATE Gdi32 User32)
endif()

# Optional: Add compiler warnings (recommended)
if(MSVC)
    target_compile_options(client PRIVATE /W4)
else()
    target_compile_options(client PRIVATE -Wall -Wextra)
endif()
