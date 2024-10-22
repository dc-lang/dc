# toolchain-arm.cmake

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Specify the Clang compiler
set(CMAKE_C_COMPILER clang-19)
set(CMAKE_CXX_COMPILER clang++-19)

# Specify the target architecture
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} --target=arm-linux-gnueabi")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --target=arm-linux-gnueabi")
