// MIT License
//
// Copyright (c) 2019 Daniel Jones
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Created by Daniel Jones on 13.07.19.
//

#include <freelist/freelist.h>

#include <functional>
#include <future>
#include <map>
#include <thread>

#include <gtest/gtest.h>

////////////////////////////////////////////////////////////////////////////////

using FreeListType = freelist::FreeList<double, 80080>;

class FreeListThreadTest : public ::testing::Test {
 protected:
  using UniquePtr = FreeListType::UniquePtr;
  FreeListType fl;
};

////////////////////////////////////////////////////////////////////////////////

std::mutex printMutex;
#define STREAM(ostream, msg)                      \
  {                                               \
    std::lock_guard<std::mutex> lock(printMutex); \
    ostream << msg;                               \
  }

bool threadFunc(FreeListType& fl, uint64_t threadNum, uint64_t doubleCount) {
  bool result = false;

  // Set the expected values for each vector item
  double d[doubleCount];
  for (int i = 0; i < doubleCount; ++i) {
    d[i] = static_cast<double>(threadNum * 100000 + i);
  }

  // Prepare a vector of unique_ptr, initially unset
  std::vector<FreeListType::UniquePtr> vec{doubleCount};

  // Loop over the array many times
  for (int j = 0; j < doubleCount * 10; ++j) {
    // Pick a pseudo-random index in the array
    uint64_t i = (j * (threadNum * (doubleCount + 1) + 1)) % doubleCount;

    // If the item is set, check its value is correct, then clear it
    if (vec[i]) {
      if (*vec[i] != d[i]) {
        // Note: STREAM() introduces a mutex, which might affect the test result
        // however, at that point the test has failed in any case.
        STREAM(std::cout, "Item " << i << " of thread " << threadNum
                                  << " was corrupted" << std::endl);
        result = true;
      }
      vec[i].reset();
    }
    // Reset the item to a new unique_ptr
    vec[i] = fl.make_unique(d[i]);
  }

  vec.clear();
  return result;
}

bool testWithNThreads(FreeListType& fl, uint64_t threadCount) {
  uint64_t doubleCount = 100;
  if (fl.max_size() <= doubleCount * threadCount) {
    std::cout << "FreeList capacity too small" << std::endl;
    return true;
  }

  std::vector<std::packaged_task<bool()>> tasks{threadCount};
  std::vector<std::thread> threads{threadCount};
  std::vector<std::future<bool>> futures{threadCount};

  for (int i = 0; i < threadCount; ++i) {
    tasks[i] = std::packaged_task<bool()>(
        std::bind(threadFunc, std::ref(fl), i, doubleCount));
    futures[i] = tasks[i].get_future();
  }

  for (int i = 0; i < threadCount; ++i) {
    threads[i] = std::thread(std::move(tasks[i]));
  }

  bool result = false;

  for (int i = 0; i < threadCount; ++i) {
    threads[i].join();
    result = result || futures[i].get();
  }
  return result;
}

TEST_F(FreeListThreadTest, twoThreads) {
  EXPECT_FALSE(testWithNThreads(fl, 2));
}

TEST_F(FreeListThreadTest, tenThreads) {
  EXPECT_FALSE(testWithNThreads(fl, 10));
}

TEST_F(FreeListThreadTest, oneHundredThreads) {
  EXPECT_FALSE(testWithNThreads(fl, 100));
}
