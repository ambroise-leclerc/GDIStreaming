cmake_minimum_required(VERSION 3.10)

# Force x64 platform on Windows before project() command
set(CMAKE_GENERATOR_PLATFORM x64)

# Project name and language
project(GrayscaleNoiseApp LANGUAGES CXX)

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Define architecture macros
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_compile_definitions(_AMD64_=1)
else()
    add_compile_definitions(_X86_=1)
endif()

# Add the client executable
add_executable(client WIN32 client.cpp)

# Add the server executable
add_executable(server WIN32 server.cpp)

# Link necessary Windows libraries for both executables
if(WIN32)
    target_link_libraries(client PRIVATE Gdi32 User32)
    target_link_libraries(server PRIVATE Gdi32 User32)
endif()

# Optional: Add compiler warnings (recommended)
if(MSVC)
    target_compile_options(client PRIVATE /W4)
    target_compile_options(server PRIVATE /W4)
else()
    target_compile_options(client PRIVATE -Wall -Wextra)
    target_compile_options(server PRIVATE -Wall -Wextra)
endif()
