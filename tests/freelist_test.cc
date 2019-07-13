//
// Copyright 2019 Daniel Jones
//
// Created by Daniel Jones on 23.06.19.
//

#include <freelist/freelist.h>

#include <map>

#include <gtest/gtest.h>

// TODO(dj) Thread-safety test

// TODO(dj) Check if virtual classes without derived data members can be stored

// TODO(dj) alloc() with args

////////////////////////////////////////////////////////////////////////////////

template <typename T>
struct TypeTraits {
  static const T zero;
  static void inc(T& t) { ++t; }
};
template <typename T>
const T TypeTraits<T>::zero = 0;

template <typename T>
struct ValueStore {
 public:
  T next() {
    T result = value;
    TypeTraits<T>::inc(value);
    return result;
  }
  T value = TypeTraits<T>::zero;
};

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
};

template <typename T>
constexpr uint64_t FreeListTest<T>::Size;

////////////////////////////////////////////////////////////////////////////////

// Define types to test with

struct complex_data {
  double d;
  float f;
  uint32_t i;
  int32_t i2;
};

template <>
struct TypeTraits<complex_data> {
  static const complex_data zero;
  static void inc(complex_data& t) {
    t.d += 1.0;
    t.f += 1.0F;
    ++t.i;
  }
};
const complex_data TypeTraits<complex_data>::zero{0.0, 0.0F, 0};

bool operator==(complex_data const& l, complex_data const& r) {
  if (l.d != r.d) return false;
  if (l.f != r.f) return false;
  if (l.i != r.i) return false;
  return true;
}

template <>
template <typename T>
struct TypeTraits<std::vector<T>> {
  static const std::vector<T> zero;
  static void inc(std::vector<T>& t) { t.push_back(T(t.size())); }
};
template <typename T>
const std::vector<T> TypeTraits<std::vector<T>>::zero{};

// Define test types

using FreeListTestTypes = ::testing::Types<
    FreeListType<int8_t, 16>, FreeListType<int8_t, 300>,
    FreeListType<int8_t, 70000>, FreeListType<double, 16>,
    FreeListType<double, 800>,
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

  for (int i = 0; i < this->fl.capacity(); ++i) {
    EXPECT_EQ(i, this->fl.size());
    indexList.push_back(this->fl.alloc());
  }
  EXPECT_EQ(this->fl.capacity(), this->fl.size());
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
    EXPECT_GE(reinterpret_cast<uint8_t const*>(indexList.back()),
              reinterpret_cast<uint8_t const*>(&this->fl));
    EXPECT_LT(reinterpret_cast<uint8_t const*>(indexList.back()),
              reinterpret_cast<uint8_t const*>(&this->fl) + TestFixture::Size);
  }
  EXPECT_THROW(indexList.push_back(this->fl.alloc()), std::bad_alloc);

  this->fl.free(indexList.back());
  indexList.pop_back();

  indexList.push_back(this->fl.alloc());
  ASSERT_NE(nullptr, indexList.back());
}

TYPED_TEST(FreeListTest, data_integrity) {
  std::vector<typename TestFixture::T*> indexList;
  std::vector<typename TestFixture::T> valueList;

  for (int i = 0; i < this->fl.capacity(); ++i) {
    indexList.push_back(this->fl.alloc());
    valueList.push_back(this->value_store.next());
    *indexList.back() = valueList.back();
  }

  for (int i = 0; i < this->fl.capacity(); ++i) {
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
    EXPECT_GE(reinterpret_cast<uint8_t const*>(indexList.back().get()),
              reinterpret_cast<uint8_t const*>(&this->fl));
    EXPECT_LT(reinterpret_cast<uint8_t const*>(indexList.back().get()),
              reinterpret_cast<uint8_t const*>(&this->fl) + TestFixture::Size);
  }
  EXPECT_THROW(indexList.push_back(this->fl.make_unique()), std::bad_alloc);

  indexList.pop_back();

  indexList.push_back(this->fl.make_unique());
  ASSERT_NE(nullptr, indexList.back().get());
}

TYPED_TEST(FreeListTest, make_shared) {
  std::vector<std::shared_ptr<typename TestFixture::T>> indexList;

  for (int i = 0; i < this->fl.capacity(); ++i) {
    indexList.push_back(this->fl.make_shared());

    // Check the pointers are within range
    EXPECT_GE(reinterpret_cast<uint8_t const*>(indexList.back().get()),
              reinterpret_cast<uint8_t const*>(&this->fl));
    EXPECT_LT(reinterpret_cast<uint8_t const*>(indexList.back().get()),
              reinterpret_cast<uint8_t const*>(&this->fl) + TestFixture::Size);
  }
  EXPECT_THROW(indexList.push_back(this->fl.make_shared()), std::bad_alloc);

  indexList.pop_back();

  indexList.push_back(this->fl.make_shared());
  ASSERT_NE(nullptr, indexList.back().get());
}
