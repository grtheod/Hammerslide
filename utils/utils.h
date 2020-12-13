#pragma once

#include <sys/time.h>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>
#include "atomic_ops.h"

//#define DEBUG
#undef DEBUG

#undef dbg
#ifdef DEBUG
#define dbg(fmt, args...)                                                      \
  do {                                                                         \
    fprintf(stdout, "DEBUG %35s (l. %4d) > " fmt, __FILE__, __LINE__, ##args); \
    fflush(stdout);                                                            \
  } while (0)
#else
#define dbg(fmt, args...)
#endif

#define info(fmt, args...)                                                     \
  do {                                                                         \
    fprintf(stdout, "INFO  %35s (l. %4d) > " fmt, __FILE__, __LINE__, ##args); \
    fflush(stdout);                                                            \
  } while (0)

#define print_error_then_terminate(en, msg) \
  do {                                      \
    errno = en;                             \
    perror(msg);                            \
    exit(EXIT_FAILURE);                     \
  } while (0)

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

#if !defined(COMPILER_NO_REORDER)
#define COMPILER_NO_REORDER(exec) \
  COMPILER_BARRIER;               \
  exec;                           \
  COMPILER_BARRIER
#endif

#define PAUSE _mm_pause()

/*
 *  If the compiler does not observe memory barriers (but any sane compiler
 *  will!), then VOLATILE should be defined as 'volatile'.
 */
#define VOLATILE volatile

#if !defined(PREFETCHW)
#define PREFETCHW(x) asm volatile("prefetchw %0" ::"m"(*(unsigned long*)x))
#endif

#if !defined(PREFETCH)
#define PREFETCH(x) asm volatile("prefetch %0" ::"m"(*(unsigned long*)x))
#endif

#define ND_GET_LOCK(nd) &nd->lock
#define DESTROY_LOCK(lock)

typedef uint64_t ticks;

static inline void cpause(ticks cycles) {
#if defined(XEON)
  cycles >>= 3;
  ticks i;
  for (i = 0; i < cycles; i++) {
    _mm_pause();
  }
#else
  ticks i;
  for (i = 0; i < cycles; i++) {
    __asm__ __volatile__("nop");
  }
#endif
}

static inline double wtime(void) {
  struct timeval t;
  gettimeofday(&t, NULL);
  return (double)t.tv_sec + ((double)t.tv_usec) / 1000000.0;
}

static inline void udelay(unsigned int micros) {
  double __ts_end = wtime() + ((double)micros / 1000000);
  while (wtime() < __ts_end)
    ;
}

static inline uint64_t rdtsc() {
  // asm("cpuid"); /* disables out of order execution engine */
  union {
    uint64_t tsc_64;
    struct {
      uint32_t lo_32;
      uint32_t hi_32;
    };
  } tsc;
  __asm__ __volatile__("rdtsc" : "=a"(tsc.lo_32), "=d"(tsc.hi_32)::);
  return tsc.tsc_64;
}

static inline void nop_rep(uint32_t num_reps) {
  uint32_t i;
  for (i = 0; i < num_reps; i++) {
    asm volatile("");
  }
}

static inline int is_power_of_two(unsigned int x) {
  return ((x != 0) && !(x & (x - 1)));
}

static inline uint32_t pow2roundup(uint32_t x) {
  if (x == 0) return 1;
  --x;
  x |= x >> 1;
  x |= x >> 2;
  x |= x >> 4;
  x |= x >> 8;
  x |= x >> 16;
  return x + 1;
}

// todo: set the cores programmatically
static std::vector<int> cores{0,  2,  4,  6,  8,  10, 12, 14, 16, 18, 20, 22,
                              24, 26, 28, 30, 32, 34, 1,  3,  5,  7,  9,  11,
                              13, 15, 17, 19, 21, 23, 25, 27, 29, 31, 33, 35};

static inline void set_cpu_manually(int core_id) {
  if (core_id >= (int)std::thread::hardware_concurrency()) {
    std::cout << "warning: the core id exceeds the number of cores" << std::endl;
    core_id = core_id % (int)std::thread::hardware_concurrency();
  }
  pthread_t pid = pthread_self();
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  const int set_result = pthread_setaffinity_np(pid, sizeof(cpu_set_t), &cpuset);
  if (set_result != 0)
    print_error_then_terminate(set_result, "sched_setaffinity");
  if (CPU_ISSET(core_id, &cpuset)) {
    // fprintf(stdout, "Successfully set thread %lu to affinity to CPU %d\n", pid, core_id);
  } else {
    fprintf(stderr, "Failed to set thread %lu to affinity to CPU %d\n", pid, core_id);
  }
}

template <typename Integer, std::enable_if_t<std::is_integral<Integer>::value, bool> = true>
static Integer getDefault() {
  return 0;
}

template <typename Floating, std::enable_if_t<std::is_floating_point<Floating>::value, bool> = true>
static Floating getDefault() {
  return 0.0;
}