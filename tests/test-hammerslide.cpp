#define CATCH_CONFIG_ENABLE_BENCHMARKING

#include <algorithm>
#include <random>

#include "catch.hpp"

#include "AggregationFunctions.hpp"
#include "HammerSlide.hpp"
#include "SystemConf.h"

TEST_CASE("HammerSlide simple testing", "[operations]") {
  SECTION("SUM operations") {
    HammerSlide<Sum<int, int, int>, SUM> hammerslide(4, 1);

    hammerslide.insert(42);
    CHECK(hammerslide.query() == 42);

    hammerslide.insert(1);
    hammerslide.insert(5);
    hammerslide.insert(2);
    CHECK(hammerslide.query() == 50);

    hammerslide.evict();
    CHECK(hammerslide.query() == 8);

    hammerslide.insert(10);
    CHECK(hammerslide.query() == 18);

    hammerslide.evict(3);
    CHECK(hammerslide.query() == 10);
  }

  SECTION("MIN operations") {
    HammerSlide<Min<int, int, int>, MIN> hammerslide(4, 1);

    hammerslide.insert(42);
    CHECK(hammerslide.query() == 42);

    hammerslide.insert(1);
    hammerslide.insert(5);
    hammerslide.insert(2);
    CHECK(hammerslide.query() == 1);

    hammerslide.evict();
    CHECK(hammerslide.query() == 1);

    hammerslide.insert(10);
    CHECK(hammerslide.query() == 1);

    hammerslide.evict(3);
    CHECK(hammerslide.query() == 10);

    hammerslide.insert(5);
    CHECK(hammerslide.query() == 5);
  }
}

TEST_CASE("HammerSlide simd testing", "[operations]") {
  SECTION("SUM operations") {
    HammerSlide<Sum<int, int, int>, SUM> hammerslide(256, 64);

    std::vector<int, tbb::cache_aligned_allocator<int>> input(64);
    int num = 0;
    for (auto& i : input) {
      i = num++;
    }

    // 0+1+...+63 = 2016
    hammerslide.insert(input.data(), 0, input.size());
    hammerslide.insert(input.data(), 0, input.size());
    hammerslide.insert(input.data(), 0, input.size());
    hammerslide.insert(input.data(), 0, input.size());
    CHECK(hammerslide.query() == 4 * 2016);

    hammerslide.evict(64);
    CHECK(hammerslide.query() == 3 * 2016);

    // add 2016+64 = 2080
    auto input1 = input;
    std::transform(std::begin(input1), std::end(input1), std::begin(input1),
                   [](int x) { return x + 1; });
    auto rng = std::default_random_engine{42};
    std::shuffle(std::begin(input1), std::end(input1), rng);
    hammerslide.insert(input1.data(), 0, input1.size());
    CHECK(hammerslide.query() == (3 * 2016 + 2080));
  }

  SECTION("MIN operations") {
    HammerSlide<Min<int, int, int>, MIN> hammerslide(256, 64);

    std::vector<int, tbb::cache_aligned_allocator<int>> input1(64);
    int num = 0;
    for (auto& i : input1) {
      i = num++;
    }
    // after suffle the minimun is still 0
    auto rng = std::default_random_engine{42};
    std::shuffle(std::begin(input1), std::end(input1), rng);
    // after suffle the minimun is 64
    auto input2 = input1;
    std::transform(std::begin(input2), std::end(input2), std::begin(input2),
                   [](int x) { return x + 64; });
    std::shuffle(std::begin(input2), std::end(input2), rng);
    // after suffle the minimun is 128
    auto input3 = input2;
    std::transform(std::begin(input3), std::end(input3), std::begin(input3),
                   [](int x) { return x + 64; });
    std::shuffle(std::begin(input3), std::end(input3), rng);
    // after suffle the minimun is 192
    auto input4 = input3;
    std::transform(std::begin(input4), std::end(input4), std::begin(input4),
                   [](int x) { return x + 64; });
    std::shuffle(std::begin(input4), std::end(input4), rng);

    hammerslide.insert(input1.data(), 0, input1.size());
    hammerslide.insert(input2.data(), 0, input2.size());
    hammerslide.insert(input3.data(), 0, input3.size());
    hammerslide.insert(input4.data(), 0, input4.size());
    CHECK(hammerslide.query() == 0);

    hammerslide.evict(64);
    CHECK(hammerslide.query() == 64);

    hammerslide.insert(input4.data(), 0, input4.size());
    CHECK(hammerslide.query() == 64);

    hammerslide.evict(64);
    CHECK(hammerslide.query() == 128);
  }
}

TEST_CASE("HammerSlide test both versions", "[operations]") {
  SECTION("SUM operations") {
    WINDOW_SIZE = 1024; WINDOW_SLIDE = 64;
    HammerSlide<Sum<int, int, int>, SUM> hammerslide(WINDOW_SIZE, WINDOW_SLIDE);

    INPUT_SIZE = 1024 * 1024;
    std::vector<int, tbb::cache_aligned_allocator<int>> input1(INPUT_SIZE);
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<int> dist(1, INPUT_SIZE);
    for (auto& i : input1) {
      i = dist(mt);
    }
    auto input2 = input1; // copy vector

    // simple version
    auto rng1 = std::default_random_engine{42};
    std::vector<int> simpleRes;
    int iter = 0;
    bool first = true;
    unsigned int idx = 0;
    size_t tuples = 0;
    while (true) {
      if (first) {
        for (; idx < WINDOW_SIZE && idx < input1.size(); ++idx) {
          hammerslide.insert(input1[idx]);
        }
        first = false;
      }

      for (; idx < input1.size();) {
        simpleRes.push_back(hammerslide.query(false));
        hammerslide.evict(WINDOW_SLIDE);
        int i = 0;
        for (; i < WINDOW_SLIDE && idx < input1.size(); ++idx) {
          hammerslide.insert(input1[idx]);
          i++;
        }
      }

      idx = 0;  // start from the beginning
      tuples += input1.size();
      if (iter++ == 100) {
        break;
      }
      std::shuffle(std::begin(input1), std::end(input1), rng1);
    }

    // reset hammerslide
    hammerslide.reset();

    // simd version
    auto rng2 = std::default_random_engine{42};
    std::vector<int> simdRes;
    iter = 0;
    first = true;
    idx = 0;
    tuples = 0;
    while (true) {
      if (first) {
        idx = std::min(WINDOW_SIZE, (unsigned int)input2.size());
        hammerslide.insert(input2.data(), 0, idx);
        first = false;
      }

      for (; idx < input2.size();) {
        // result += hammerslide.query();
        simdRes.push_back(hammerslide.query());
        hammerslide.evict(WINDOW_SLIDE);
        auto next_idx = std::min(idx + WINDOW_SLIDE, (unsigned int)input2.size());
        hammerslide.insert(input2.data(), idx, next_idx);
        idx = next_idx;
      }

      idx = 0;  // start from the beginning
      tuples += input2.size();
      if (iter++ == 100) {
        break;
      }
      std::shuffle(std::begin(input2), std::end(input2), rng2);
    }

    // Compare all the elements of two vectors
    bool equal = std::equal(simdRes.begin(), simdRes.end(), simpleRes.begin());
    CHECK(equal == true);
  }

  SECTION("MIN operations") {
    WINDOW_SIZE = 1024; WINDOW_SLIDE = 64;
    HammerSlide<Min<int, int, int>, MIN> hammerslide(WINDOW_SIZE, WINDOW_SLIDE);

    INPUT_SIZE = 1024 * 1024;
    std::vector<int, tbb::cache_aligned_allocator<int>> input1(INPUT_SIZE);
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<int> dist(1, INPUT_SIZE);
    for (auto& i : input1) {
      i = dist(mt);
    }
    auto input2 = input1;

    // simple version
    auto rng1 = std::default_random_engine{42};
    std::vector<int> simpleRes;
    int iter = 0;
    bool first = true;
    unsigned int idx = 0;
    size_t tuples = 0;
    while (true) {
      if (first) {
        for (; idx < WINDOW_SIZE && idx < input1.size(); ++idx) {
          hammerslide.insert(input1[idx]);
        }
        first = false;
      }

      for (; idx < input1.size();) {
        simpleRes.push_back(hammerslide.query(false));
        hammerslide.evict(WINDOW_SLIDE);
        int i = 0;
        for (; i < WINDOW_SLIDE && idx < input1.size(); ++idx) {
          hammerslide.insert(input1[idx]);
          i++;
        }
      }

      idx = 0;  // start from the beginning
      tuples += input1.size();
      if (iter++ == 100) {
        break;
      }
      std::shuffle(std::begin(input1), std::end(input1), rng1);
    }

    // reset hammerslide
    hammerslide.reset();

    // simd version
    auto rng2 = std::default_random_engine{42};
    std::vector<int> simdRes;
    iter = 0;
    first = true;
    idx = 0;
    tuples = 0;
    while (true) {
      if (first) {
        idx = std::min(WINDOW_SIZE, (unsigned int)input2.size());
        hammerslide.insert(input2.data(), 0, idx);
        first = false;
      }

      for (; idx < input2.size();) {
        // result += hammerslide.query();
        simdRes.push_back(hammerslide.query());
        hammerslide.evict(WINDOW_SLIDE);
        auto next_idx = std::min(idx + WINDOW_SLIDE, (unsigned int)input2.size());
        hammerslide.insert(input2.data(), idx, next_idx);
        idx = next_idx;
      }

      idx = 0;  // start from the beginning
      tuples += input2.size();
      if (iter++ == 100) {
        break;
      }
      std::shuffle(std::begin(input2), std::end(input2), rng2);
    }

    // Compare all the elements of two vectors
    bool equal = std::equal(simdRes.begin(), simdRes.end(), simpleRes.begin());
    CHECK(equal == true);
  }
}