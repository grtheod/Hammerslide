cmake_minimum_required(VERSION 3.15)
project(HammerSlide)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CPP_FILES utils/SystemConf.h)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing -Wall -DTAS -msse2 -mfpmath=sse -march=native -mtune=native")

FIND_LIBRARY(tbb NAMES libtbb.so)

add_subdirectory(tests)

add_executable(hammerslide-bench main.cpp ${CPP_FILES})
target_link_libraries(hammerslide-bench -lpthread -lm -ltbb)
target_compile_options(hammerslide-bench PRIVATE -O3 -mrtm -frename-registers -funroll-loops -flto) #-falign-loops=15 -falign-labels=15)

#-fno-tree-vectorize