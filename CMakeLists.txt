cmake_minimum_required(VERSION 3.8)
project(dc)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SOURCES "src/dcc.cpp" "src/args.cpp" "src/fs.cpp" "src/lexer.cpp" "src/compiler.cpp")

if(DEFINED LLVM_INCLUDE_DIRS)
  include_directories(${LLVM_INCLUDE_DIRS})
endif()
if(DEFINED LLVMC_INCLUDE_DIRS)
  include_directories(${LLVMC_INCLUDE_DIRS})
endif()

add_executable("dcc" ${SOURCES})
target_link_libraries("dcc" LLVM-19)
target_include_directories("dcc" PUBLIC "include")
