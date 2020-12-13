cmake_minimum_required(VERSION 3.15)
project(tests)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_VERBOSE_MAKEFILE ON)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fno-strict-aliasing -Wall -msse2 -mfpmath=sse -march=native -mtune=native")

add_executable(hammerslide-test test-main.cpp test-hammerslide.cpp)
target_include_directories(hammerslide-test PRIVATE
        ${CMAKE_HOME_DIRECTORY}/ ${CMAKE_HOME_DIRECTORY}/utils)
target_link_libraries(hammerslide-test -lpthread -lm -ltbb)
target_compile_options(hammerslide-test PRIVATE -O0 -mrtm)