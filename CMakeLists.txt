# --- Project Setup ---
# Specifies the minimum version of CMake required and defines the project.
cmake_minimum_required(VERSION 3.21)
project(inevitable LANGUAGES CXX)

# --- C++ Standard Configuration ---
# Set the C++ standard to C++20, which corresponds to Visual Studio's "stdcpplatest".
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Executable Definition ---
# Create the main executable and add all source files directly.
add_executable(inevitable
    main.cpp
    CPU.cpp
    Process.cpp
    InterruptController.cpp
    algo/FCFSScheduler.cpp
    algo/SJFScheduler.cpp
    algo/SRTFScheduler.cpp
    algo/PriorityScheduler.cpp
)

# --- Include Directories ---
# Add the project's root directory to the include path.
target_include_directories(inevitable PRIVATE ${PROJECT_SOURCE_DIR})

# --- Compiler Options ---
# Set compiler-specific warning levels to match your Visual Studio settings.
if(MSVC)
  # Corresponds to /W4 in Visual Studio
  target_compile_options(inevitable PRIVATE /W4)
else()
  # Standard flags for GCC and Clang
  find_package(Threads REQUIRED)
  target_compile_options(inevitable PRIVATE -Wall -Wextra -Wpedantic Threads::Threads)
endif()