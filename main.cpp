#include <sys/mman.h>
#include <chrono>
#include <iostream>
#include <random>

#include "HammerSlide.hpp"
#include "utils/AggregationFunctions.hpp"
#include "utils/SystemConf.h"
#include "utils/utils.h"

static volatile size_t result = 0;

int main(int argc, const char** argv) {
  parseCLArgs(argc, argv);

  // bind the process to one core
  const int core_id = 1;
  set_cpu_manually(core_id);

  // initialize Hammerslide
  HammerSlide<Sum<int, int, int>, SUM> hammerslide(WINDOW_SIZE, WINDOW_SLIDE);

  // use a cache-aligned input vector
  std::vector<int, tbb::cache_aligned_allocator<int>> input(INPUT_SIZE);

  // generate random ints
  std::random_device rd;
  std::mt19937 mt(rd());
  std::uniform_int_distribution<int> dist(1, INPUT_SIZE * 2);
  for (auto& i : input) {
    i = dist(mt);
  }

  // measure simple operations
  bool first = true;
  unsigned int idx = 0;
  size_t tuples = 0;
  auto t1 = std::chrono::high_resolution_clock::now();
  auto t2 = t1;
  auto time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
  while (true) {
    if (first) {
      for (; idx < WINDOW_SIZE && idx < input.size(); ++idx) {
        hammerslide.insert(input[idx]);
      }
      first = false;
    }

    for (; idx < input.size();) {
      result += hammerslide.query(false);
      hammerslide.evict(WINDOW_SLIDE);
      int i = 0;
      for (; i < WINDOW_SLIDE && idx < input.size(); ++idx) {
        hammerslide.insert(input[idx]);
        i++;
      }
    }

    idx = 0;  // start from the beginning
    tuples += input.size();
    t2 = std::chrono::high_resolution_clock::now();
    time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
    if (time_span.count() >= (double)DURATION / 1000) {
      break;
    }
  }
  std::cout << "Throughput: " << tuples / time_span.count() << " tuples/sec ("
            << result << ")" << std::endl;

  // reset hammerslide
  hammerslide.reset();

  // measure simd operations
  first = true;
  result = 0;
  idx = 0;
  tuples = 0;
  t1 = std::chrono::high_resolution_clock::now();
  t2 = t1;
  time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
  while (true) {
    if (first) {
      idx = std::min(WINDOW_SIZE, (unsigned int)input.size());
      hammerslide.insert(input.data(), 0, idx);
      first = false;
    }

    for (; idx < input.size();) {
      result += hammerslide.query();
      hammerslide.evict(WINDOW_SLIDE);
      auto next_idx = std::min(idx + WINDOW_SLIDE, (unsigned int)input.size());
      hammerslide.insert(input.data(), idx, next_idx);
      idx = next_idx;
    }

    idx = 0;  // start from the beginning
    tuples += input.size();
    t2 = std::chrono::high_resolution_clock::now();
    time_span = std::chrono::duration_cast<std::chrono::duration<double>>(t2 - t1);
    if (time_span.count() >= (double)DURATION / 1000) {
      break;
    }
  }
  std::cout << "Throughput with SIMD: " << tuples / time_span.count()
            << " tuples/sec (" << result << ")" << std::endl;
  return 0;
}
