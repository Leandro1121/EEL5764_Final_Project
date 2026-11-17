cmake_minimum_required(VERSION 3.10)
project(Final_Project C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(BENCH_FLAGS -O2 -fno-tree-vectorize -Wall)

# Adding all test executables
set(TEST_PROGRAMS 
  branch
  chase
  gather
  scatter
)

foreach(prog IN LISTS TEST_PROGRAMS)
  add_executable(${prog} "src/${prog}.c")
  target_compile_options(${prog} PRIVATE ${BENCH_FLAGS})
  target_link_options(${prog} PRIVATE -static) 
endforeach()


