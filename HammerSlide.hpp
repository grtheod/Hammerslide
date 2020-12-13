#pragma once

#include <climits>
#include <vector>

#include "emmintrin.h"
#include "immintrin.h"

#include "CircularQueue.hpp"
#include "utils/SystemConf.h"

typedef union {
  __m256i v;
  int a[8];
} U256i;

typedef union {
  __m128i v;
  int a[4];
} U128i;

template <typename AggrFun, AggregationType type>
struct alignas(64) HammerSlide {
  typedef typename AggrFun::In inT;
  typedef typename AggrFun::Partial aggT;
  typedef typename AggrFun::Out outT;

  int m_windowSize;
  int m_windowSlide;
  int m_windowPane;
  int m_currentWindowPane;
  int m_countBasedCounter;

  // variables for emulating a stack
  size_t m_capacity;
  int m_istackSize;
  int m_istackPtr;
  int m_ostackSize;
  int m_ostackPtr;
  aggT m_istackVal;

  // a queue that holds the actual data
  CircularQueue<inT> m_queue;
  // a variable with the aggT of the input stack
  aggT m_runningValue;
  // a vector with the aggTs of the output stack
  std::vector<aggT, tbb::cache_aligned_allocator<aggT>> m_ostackVal;
  AggrFun m_op;

  HammerSlide(int windowSize, int windowSlide)
      : m_windowSize(windowSize),
        m_windowSlide(windowSlide),
        m_capacity(0),
        m_istackSize(0),
        m_istackPtr(-1),
        m_ostackSize(0),
        m_ostackPtr(-1),
        m_queue(windowSize),
        m_ostackVal(windowSize) {
    m_windowPane = m_windowSize / m_windowSlide;  // fix: assume that the slide is a multiple of the size
    m_currentWindowPane = 0;
    m_countBasedCounter = 0;
    m_runningValue = m_op.identity;
  };

  inline void insert(inT val) {
    aggT tempValue = (m_istackSize == 0) ? m_op.identity : m_istackVal;
    m_istackVal = m_op.combine(m_op.lift(val), tempValue);
    m_queue.enqueue(val);
    m_istackPtr = m_queue.m_rear;
    m_capacity++;
    m_istackSize++;
  }

  inline void insert(inT* vals, int start, int end) {
    auto numOfVals = end - start;
    if (m_windowSlide < 16 || numOfVals < 16) {  // skip vectorization for less than 16 integers
      insert_simple_range(vals, start, end);
    } else {
      // assume 256bit-length instructions cache-aligned
      // todo: generalize this for different lengths
      int roundedStart = (start % 8 == 0) ? start : start + (8 - start % 8);
      int roundedEnd = (end % 8 == 0) ? end : end + (-end % 8);
      int diff = roundedEnd - roundedStart;
      int n = (diff / 8);

      // compute the first elements that don't fit in the 256-bit vectors
      if (start != roundedStart) {
        int tempRoundedStart = roundedStart;
        if (n > 2) tempRoundedStart--;
        insert_simple_range(vals, start, tempRoundedStart);
      }

      // compute the elements that can be vectorized
      if (n > 0) {
        switch (type) {
          case MIN: {
            aggT tempMin = (m_istackSize == 0) ? m_op.identity : m_istackVal;

            int startingPos = roundedStart / 8;
            __m256i* f4 = (__m256i*)vals;
            __m256i minVal1 = _mm256_set1_epi32(m_op.identity);
            for (int i = startingPos; i < startingPos + n; i++) {
              minVal1 = _mm256_min_epi32(minVal1, f4[i]);
            }
            const U256i ins = {minVal1};
            for (int i = 0; i < 8; i++) {
              tempMin = (ins.a[i] < tempMin) ? ins.a[i] : tempMin;
            }

            m_istackVal = tempMin;

          } break;
          case SUM: {
            int startingPos = roundedStart / 8;
            __m256i* f4 = (__m256i*)vals;
            __m256i tempVal = _mm256_set1_epi32(0);
            for (int i = startingPos; i < startingPos + n; i++) {
              tempVal = _mm256_add_epi32(tempVal, f4[i]);
            }
            tempVal = _mm256_hadd_epi32(tempVal, tempVal);
            tempVal =
                _mm256_add_epi32(tempVal, _mm256_permute2f128_si256(tempVal, tempVal, 0x1));
            __m128i tempSum = _mm_hadd_epi32(_mm256_castsi256_si128(tempVal),
                                             _mm256_castsi256_si128(tempVal));

            const U128i ins = {tempSum};
            m_istackVal = ((m_istackSize == 0) ? 0 : m_istackVal) + ins.a[0];
          } break;
          case MAX:
          case CNT:
          case AVG:
          default:
            throw std::runtime_error("error: operation not supported yet");
        }

        // enqueue data in the circular buffer in bulk
        m_queue.enqueue_many(vals, start, end);
        m_capacity += diff;
        m_istackPtr = m_queue.m_rear;
        m_istackSize += diff;
      }

      // compute the remaining elements
      if (end != roundedEnd) {
        insert_simple_range(vals, roundedEnd, end);
      }
    }
  }

  inline void evict(int numberOfItems = 1) {
    m_ostackPtr += numberOfItems;
    m_ostackSize -= numberOfItems;
    m_capacity -= numberOfItems;
    m_queue.dequeue_many(numberOfItems);
  }

  inline outT query(bool isSIMD = true) {
    if (m_ostackSize == 0) {
      swap(isSIMD);
    }

    aggT temp1 = m_ostackVal[m_ostackSize - 1];
    aggT temp2 = (m_istackSize == 0) ? m_op.identity : m_istackVal;

    // todo: fix aggregates like avg
    return m_op.lower(m_op.combine(temp1, temp2));
  }

  inline void reset() {
    m_capacity = 0;
    m_istackSize = 0;
    m_istackPtr = -1;
    m_ostackSize = 0;
    m_ostackPtr = -1;
    m_queue.reset();
  }

  /* helper functions */
  inline void insert_simple_range(inT* vals, int start, int end) {
    auto numOfVals = end - start;
    aggT tempValue = (m_istackSize == 0) ? m_op.identity : m_istackVal;
    for (int i = start; i < end; i++) {
      tempValue = m_op.combine(m_op.lift(vals[i]), tempValue);
      m_queue.enqueue(vals[i]);
    }
    m_istackPtr = m_queue.m_rear;
    m_capacity += numOfVals;
    m_istackSize += numOfVals;
    m_istackVal = tempValue;
  }

  /*
   * todo: the swap function has been tested with window sizes/slides that are a power of two.
   * */
  inline void swap(bool isSIMD = true) {
    int outputIndex = 0;
    int inputIndex = m_istackPtr;
    int limit = m_istackSize;
    int tempRear = m_queue.m_rear;
    int queueSize = m_queue.m_size;

    aggT tempValue = m_op.identity;
    if (m_windowSlide < 16 || !isSIMD) {  // skip vectorization for less than 16 integers
      for (outputIndex = 0; outputIndex < limit; outputIndex++) {
        auto tempTuple = m_queue.m_arr[inputIndex];
        tempValue = m_op.combine(tempTuple, tempValue);
        m_ostackVal[outputIndex] = tempValue;
        inputIndex--;
      }
    } else {  // SIMD path
      // The logic of this code is that we start iterating the first stack
      // stored in the circular buffer backwards based on the window slide
      // and compute the aggregate values. We round the limits of each window
      // slide within the circular buffer so that they fit in 256-bit vectors.
      // Todo: simplify the logic of the following code...
      int windowSize = m_istackSize;
      int writePosition = 0;
      int tempSize = 0;
      while (tempSize < windowSize) {
        // int tempQueueFront = (tempRear - m_istackSize + 1 + tempSize) % queueSize;
        // int tempQueueRear = (tempQueueFront + m_windowSlide - 1) % queueSize;
        int tempQueueRear = tempRear - tempSize;
        int tempQueueFront = tempQueueRear - m_windowSlide + 1;
        if (tempQueueRear >= queueSize || tempQueueFront >= queueSize) {
          throw std::runtime_error("error: wrong queue indexes");
        }

        int roundedFront = (tempQueueFront % 8 == 0)
                               ? tempQueueFront
                               : (tempQueueFront) + (8 - (tempQueueFront) % 8);
        int roundedRear = ((tempQueueRear + 1) % 8 == 0)
                              ? tempQueueRear
                              : (tempQueueRear) + (-(tempQueueRear) % 8);

        int diff1 = roundedRear - roundedFront;
        int diff2 = (queueSize - 1) - roundedFront;
        int diff3 = roundedRear;

        int n1 = 0;
        int n2 = 0;
        int endFront = 0;  // between initial poisition and rounded front
        int endRear = 0;   // between the last rounded position and rear
        int startRear = 0;
        if (diff1 >= 0) {
          n1 = (diff1 >= 8) ? ((diff1 + 1) / 8) : 0;  // skip the case when n = 1
          endFront = (n1 < 2) ? roundedFront + diff1 : roundedFront;
          if (endFront != tempQueueFront) {
            if (n1 >= 2) endFront--;
            for (int i = tempQueueFront; i <= endFront; i++) {
              tempValue = m_op.combine(m_op.lift(m_queue.m_arr[i]), tempValue);
            }
          }
          if (tempQueueRear != roundedRear) {
            startRear = roundedRear;
            if (roundedRear == endFront || n1 >= 2) startRear++;
            for (int i = startRear; i <= tempQueueRear; i++) {
              tempValue = m_op.combine(m_op.lift(m_queue.m_arr[i]), tempValue);
            }
          }
        } else {
          n1 = (diff2 >= 8) ? (diff2 / 8) : 0;  // skip the case when n = 1
          endFront = (n1 < 2) ? roundedFront + diff2 - 1 : roundedFront;
          n2 = (diff3 >= 8) ? ((diff3 + 1) / 8) : 0;  // skip the case when n = 1
          endRear = (n2 < 2) ? tempQueueRear : roundedRear;
          if (endFront != tempQueueFront) {
            if (n1 >= 2) endFront--;
            for (int i = tempQueueFront; i <= endFront; i++) {
              tempValue = m_op.combine(m_op.lift(m_queue.m_arr[i]), tempValue);
            }
          }
          if (endRear != roundedRear)
            for (int i = roundedRear; i <= endRear; i++) {
              tempValue = m_op.combine(m_op.lift(m_queue.m_arr[i]), tempValue);
            }
        }

        // between front and rear or size
        switch (type) {
          case MIN: {
            if (n1 > 1) {
              int start = roundedFront / 8;
              __m256i* f4 = (__m256i*)m_queue.m_arr.data();
              __m256i minVal1 = _mm256_set1_epi32(INT_MAX);
              for (int i = start; i < start + n1; i++) {
                minVal1 = _mm256_min_epi32(minVal1, f4[i]);
              }
              const U256i r1 = {minVal1};
              for (int i = 0; i < 8; i++) {
                tempValue = m_op.combine(m_op.lift(r1.a[i]), tempValue);
              }
            }

            // between start and rear
            if (n2 > 1) {
              int start = roundedRear / 8;
              __m256i* f4 = (__m256i*)m_queue.m_arr.data();
              __m256i minVal2 = _mm256_set1_epi32(INT_MAX);
              for (int i = 0; i < n2; i++) {
                minVal2 = _mm256_min_epi32(minVal2, f4[i]);
              }
              const U256i r2 = {minVal2};
              for (int i = 0; i < 8; i++) {
                tempValue = m_op.combine(m_op.lift(r2.a[i]), tempValue);
              }
            }
          } break;
          case SUM: {
            // between front and rear or size
            if (n1 > 1) {
              int start = roundedFront / 8;
              __m256i* f4 = (__m256i*)m_queue.m_arr.data();
              __m256i tempVal1 = _mm256_set1_epi32(0);
              for (int i = start; i < start + n1; i++) {
                tempVal1 = _mm256_add_epi32(tempVal1, f4[i]);
              }
              tempVal1 = _mm256_hadd_epi32(tempVal1, tempVal1);
              tempVal1 =
                  _mm256_add_epi32(tempVal1,
                                   _mm256_permute2f128_si256(tempVal1, tempVal1, 0x1));
              __m128i tempSum1 = _mm_hadd_epi32(_mm256_castsi256_si128(tempVal1),
                                                _mm256_castsi256_si128(tempVal1));

              const U128i r1 = {tempSum1};
              tempValue += r1.a[0];
            }

            // between start and rear
            if (n2 > 1) {
              roundedRear++;
              int start = roundedRear / 8;
              __m256i* f4 = (__m256i*)m_queue.m_arr.data();
              __m256i tempVal2 = _mm256_set1_epi32(0);
              for (int i = 0; i < n2; i++) {
                tempVal2 = _mm256_add_epi32(tempVal2, f4[i]);
              }
              tempVal2 = _mm256_hadd_epi32(tempVal2, tempVal2);
              tempVal2 =
                  _mm256_add_epi32(tempVal2,
                                   _mm256_permute2f128_si256(tempVal2, tempVal2, 0x1));
              __m128i tempSum2 = _mm_hadd_epi32(_mm256_castsi256_si128(tempVal2),
                                                _mm256_castsi256_si128(tempVal2));

              const U128i r2 = {tempSum2};
              tempValue += r2.a[0];
            }
          } break;
          case MAX:
          case CNT:
          case AVG:
          default:
            throw std::runtime_error("error: operation not supported yet");
        }

        writePosition += m_windowSlide;
        m_ostackVal[writePosition - 1] = tempValue;
        tempSize += m_windowSlide;
      }
    }

    m_ostackSize = limit;
    m_istackSize = 0;
    m_ostackPtr = m_queue.m_rear - m_ostackSize + 1;
    if (m_ostackPtr < 0)
      m_ostackPtr = (m_queue.m_rear + m_ostackSize) % (m_queue.m_size - 1);
    m_istackPtr = -1;
  }
};