# toolchain-arm.cmake

# Set the C and C++ compilers
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Specify the cross compiler
set(CMAKE_C_COMPILER arm-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnu-g++)
