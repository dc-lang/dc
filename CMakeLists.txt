cmake_minimum_required(VERSION 3.8)
project(dc)

set(CMAKE_CXX_STANDARD 20)

set(SOURCES "src/dcc.cpp" "src/args.cpp" "src/fs.cpp" "src/lexer.cpp" "src/compiler.cpp")

find_package(LLVM 14 REQUIRED CONFIG)
include_directories(${LLVM_INCLUDE_DIRS})

add_executable("dcc" ${SOURCES})
target_link_libraries("dcc" LLVM-14)
target_include_directories("dcc" PUBLIC "include")
