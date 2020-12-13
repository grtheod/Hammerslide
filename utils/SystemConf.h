#pragma once

#include <cstring>
#include <iostream>
#include <stdexcept>

enum AggregationType { MIN, MAX, CNT, SUM, AVG };

enum TimeGranularity { sec, msec, nsec };

static unsigned int WORKER_THREADS = 1;
static unsigned int WINDOW_SIZE = 1024;
static unsigned int WINDOW_SLIDE = 64;
static unsigned int DURATION = 4000;
static unsigned int INPUT_SIZE = 16 * 1024 * 1024;
static AggregationType TYPE = MIN;

static inline void parseCLArgs(int argc, const char** argv) {
  int i, j;
  for (i = 1; i < argc;) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      std::cout << "Options:\n"
                   "  -h, --help\n"
                   "    Print this message\n"
                   "  --duration <int>\n"
                   "    Test duration in milliseconds\n"
                   "  --size <int>\n"
                   "    Window size in tuples\n"
                   "  --slide <int>\n"
                   "    Window slide int tuples\n"
                   "  --input <int>\n"
                   "    Input size in tuples\n"
                //"  --type fun\n"
                //"    Choose fun from [MIN, SUM]\n"
                << std::endl;
    }

    if ((j = i + 1) == argc) {
      throw std::runtime_error("error: wrong number of arguments");
    }

    if (strcmp(argv[i], "--size") == 0) {
      WINDOW_SIZE = std::atoi(argv[j]);
    } else if (strcmp(argv[i], ("--slide")) == 0) {
      WINDOW_SLIDE = std::atoi(argv[j]);
    } else if (strcmp(argv[i], ("--duration")) == 0) {
      DURATION = std::atoi(argv[j]);
    } else if (strcmp(argv[i], ("--input")) == 0) {
      INPUT_SIZE = std::atoi(argv[j]);
    } else if (strcmp(argv[i], ("--type")) == 0) {
      if (strcmp(argv[j], ("MIN")) == 0) {
        TYPE = MIN;
      } else if (strcmp(argv[j], ("SUM")) == 0) {
        TYPE = SUM;
      } else {
        throw std::runtime_error("error: operation not supported yet");
      }
    } else {
      throw std::runtime_error("error: invalid argument");
    }
    i = j + 1;
  }
}