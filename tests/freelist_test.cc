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
// Created by Daniel Jones on 23.06.19.
//

#include <freelist/freelist.h>

#include <algorithm>
#include <map>

#include <gtest/gtest.h>

#include "test_values.h"

// TODO(dj) Check if virtual classes without derived data members can be stored

// TODO(dj) alloc() with args

////////////////////////////////////////////////////////////////////////////////
// Set up test fixture for typed tests

template <typename _T, uint64_t Size>
struct FreeListType {
  using T = _T;
  static constexpr uint64_t size = Size;
};

template <typename _T>
class FreeListTest : public ::testing::Test {
 protected:
  using this_FreeList = freelist::FreeList<typename _T::T, _T::size>;
  using T = typename _T::T;
  static constexpr uint64_t Size = _T::size;

  this_FreeList fl;
  ValueStore<typename _T::T> value_store;

  void checkPointer(T* item) {
    EXPECT_FALSE(item == nullptr);

    EXPECT_LE(reinterpret_cast<uint8_t const*>(&fl),
              reinterpret_cast<uint8_t const*>(item));
    EXPECT_GT(reinterpret_cast<uint8_t const*>(&fl) + Size,
              reinterpret_cast<uint8_t const*>(item));
  }
};

template <typename T>
constexpr uint64_t FreeListTest<T>::Size;

////////////////////////////////////////////////////////////////////////////////

// Define types to test with

// Define test types for edge cases
using FreeListTestTypes = ::testing::Types<
    FreeListType<int8_t, 8>,         // Minimum size for 1-byte type
    FreeListType<int16_t, 8>,        // Minimum size for 2-byte type
    FreeListType<int32_t, 8>,        // Minimum size for 4-byte type
    FreeListType<int64_t, 16>,       // Minimum size for 8-byte type
    FreeListType<float, 8>,          // Minimum size for float
    FreeListType<double, 16>,        // Minimum size for double
    FreeListType<float, 256>,        // Largest freelist with 1-byte indexes
    FreeListType<double, 264>,       // Smallest freelist with 2-byte indexes
    FreeListType<double, 131072>,    // Largest freelist with 2-byte indexes
    FreeListType<double, 131088>,    // Smallest freelist with 4-byte indexes
    FreeListType<double, 16777216>,  // 16MB freelist with 4-byte indexes
    // Note: the largest FreeList possible with 4-byte indexes is 16GB.
    // The smallest FreeList requiring 8-byte indexes is larger than 16GB, so
    // it is not tested.

    // Test abnormal-sized data structures, with different index sizes
    FreeListType<AbnormalSize<3>, 512>, FreeListType<AbnormalSize<3>, 131088>,
    FreeListType<AbnormalSize<7>, 16>, FreeListType<AbnormalSize<7>, 16000>,
    FreeListType<AbnormalSize<7>, 131088>, FreeListType<AbnormalSize<15>, 32>,
    FreeListType<AbnormalSize<15>, 32000>,
    FreeListType<AbnormalSize<15>, 131088>,

    // Test complex data structures
    FreeListType<std::string, sizeof(std::string) * 100>,
    FreeListType<complex_data, sizeof(complex_data) * 100>,
    FreeListType<std::vector<int>, sizeof(std::vector<int>) * 100>>;

TYPED_TEST_CASE(FreeListTest, FreeListTestTypes);

////////////////////////////////////////////////////////////////////////////////

TYPED_TEST(FreeListTest, sizeof_) {
  EXPECT_EQ(TestFixture::Size, sizeof(this->fl));
}

TYPED_TEST(FreeListTest, empty) {
  std::vector<typename TestFixture::T*> indexList;

  EXPECT_TRUE(this->fl.empty());
  indexList.push_back(this->fl.alloc());
  EXPECT_FALSE(this->fl.empty());
  this->fl.free(indexList.back());
  indexList.pop_back();
  EXPECT_TRUE(this->fl.empty());
}

TYPED_TEST(FreeListTest, full) {
  std::vector<typename TestFixture::T*> indexList;

  for (int i = 0; i < this->fl.capacity(); ++i) {
    EXPECT_FALSE(this->fl.full());
    indexList.push_back(this->fl.alloc());
  }
  EXPECT_TRUE(this->fl.full());

  this->fl.free(indexList.back());
  indexList.pop_back();
  EXPECT_FALSE(this->fl.full());
}

TYPED_TEST(FreeListTest, size) {
  std::vector<typename TestFixture::T*> indexList;

  const int numItems = std::min(10000, int(this->fl.capacity()));
  for (int i = 0; i < numItems; ++i) {
    EXPECT_EQ(i, this->fl.size());
    indexList.push_back(this->fl.alloc());
  }
  EXPECT_EQ(numItems, this->fl.size());
}

TYPED_TEST(FreeListTest, capacity) {
  std::vector<typename TestFixture::T*> indexList;

  EXPECT_GT(TestFixture::Size / sizeof(typename TestFixture::T),
            this->fl.capacity());
}

TYPED_TEST(FreeListTest, alloc) {
  std::vector<typename TestFixture::T*> indexList;

  for (int i = 0; i < this->fl.capacity(); ++i) {
    indexList.push_back(this->fl.alloc());

    // Check the pointers are within range
    TestFixture::checkPointer(indexList.back());
  }
  EXPECT_THROW(indexList.push_back(this->fl.alloc()), std::bad_alloc);

  this->fl.free(indexList.back());
  indexList.pop_back();

  indexList.push_back(this->fl.alloc());
  TestFixture::checkPointer(indexList.back());
}

TYPED_TEST(FreeListTest, data_integrity) {
  std::vector<typename TestFixture::T*> indexList;
  std::vector<typename TestFixture::T> valueList;

  const int numItems = std::min(10000, int(this->fl.capacity()));
  for (int i = 0; i < numItems; ++i) {
    indexList.push_back(this->fl.alloc());
    valueList.push_back(this->value_store.next());
    *indexList.back() = valueList.back();
  }

  for (int i = 0; i < numItems; ++i) {
    EXPECT_EQ(valueList[i], *indexList[i]);
  }
}

TYPED_TEST(FreeListTest, alloc_and_free) {
  std::vector<typename TestFixture::T> valueList;

  // TODO change test to cover lists with smaller capacity

  if (this->fl.capacity() >= 6) {
    typename TestFixture::T* d0 = this->fl.alloc();
    valueList.push_back(this->value_store.next());
    *d0 = valueList.back();
    typename TestFixture::T* d1 = this->fl.alloc();
    valueList.push_back(this->value_store.next());
    *d1 = valueList.back();
    typename TestFixture::T* d2 = this->fl.alloc();
    valueList.push_back(this->value_store.next());
    *d2 = valueList.back();
    typename TestFixture::T* dm1 = this->fl.alloc();
    *dm1 = this->value_store.next();
    typename TestFixture::T* dm2 = this->fl.alloc();
    *dm2 = this->value_store.next();
    typename TestFixture::T* dm3 = this->fl.alloc();
    *dm3 = this->value_store.next();

    this->fl.free(dm1);
    this->fl.free(dm2);

    typename TestFixture::T* d3 = this->fl.alloc();
    valueList.push_back(this->value_store.next());
    *d3 = valueList.back();

    this->fl.free(dm3);

    typename TestFixture::T* dm4 = this->fl.alloc();
    *dm3 = this->value_store.next();

    this->fl.free(dm4);

    EXPECT_EQ(valueList[0], *d0);
    EXPECT_EQ(valueList[1], *d1);
    EXPECT_EQ(valueList[2], *d2);
    EXPECT_EQ(valueList[3], *d3);

    this->fl.free(d0);
    this->fl.free(d1);
    this->fl.free(d2);
    this->fl.free(d3);
  }
}

TYPED_TEST(FreeListTest, make_unique) {
  std::vector<typename TestFixture::this_FreeList::UniquePtr> indexList;

  for (int i = 0; i < this->fl.capacity(); ++i) {
    indexList.push_back(this->fl.make_unique());

    // Check the pointers are within range
    TestFixture::checkPointer(indexList.back().get());
  }
  EXPECT_THROW(indexList.push_back(this->fl.make_unique()), std::bad_alloc);

  indexList.pop_back();

  indexList.push_back(this->fl.make_unique());
  TestFixture::checkPointer(indexList.back().get());
}

TYPED_TEST(FreeListTest, make_shared) {
  std::vector<std::shared_ptr<typename TestFixture::T>> indexList;

  for (int i = 0; i < this->fl.capacity(); ++i) {
    indexList.push_back(this->fl.make_shared());

    // Check the pointers are within range
    TestFixture::checkPointer(indexList.back().get());
  }
  EXPECT_THROW(indexList.push_back(this->fl.make_shared()), std::bad_alloc);

  indexList.pop_back();

  indexList.push_back(this->fl.make_shared());
  TestFixture::checkPointer(indexList.back().get());
}

TYPED_TEST(FreeListTest, vector_allocator) {
  std::vector<typename TestFixture::T,
              typename TestFixture::this_FreeList::Allocator>
      vec(this->fl.allocator());

  EXPECT_EQ(0, this->fl.size());

  vec.push_back(this->value_store.next());
  EXPECT_EQ(1, this->fl.size());

  vec.pop_back();
  vec.shrink_to_fit();
  EXPECT_EQ(0, this->fl.size());
}
