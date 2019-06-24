//
// Created by Daniel Jones on 23.06.19.
//

#include <freelist/freelist.h>

#include <gtest/gtest.h>

////////////////////////////////////////////////////////////////////////////////

template <typename T>
struct TypeTraits {
  static const T zero;
  static void inc(T& t) { ++t; }
};
template <typename T>
const T TypeTraits<T>::zero = 0;


template<typename T>
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

template<typename _T, uint64_t Size>
struct FreeListType {
  using T = _T;
  static constexpr uint64_t size = Size;
};

template<typename _T>
class FreeListTest : public ::testing::Test {
 protected:


  using this_FreeList = FreeList<typename _T::T, _T::size>;
  using T = typename _T::T;
  static constexpr uint64_t Size = _T::size;

  this_FreeList freelist;
  ValueStore<typename _T::T> value_store;
};

template<typename T>
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
  static void inc(complex_data& t) { t.d += 1.0; t.f += 1.0F; ++t.i; }
};
const complex_data TypeTraits<complex_data>::zero{0.0, 0.0F, 0};

bool operator==(complex_data const& l, complex_data const& r)
{
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
    FreeListType<int8_t, 16>,
    FreeListType<int8_t, 300>,
    FreeListType<int8_t, 70000>,
    FreeListType<double, 16>,
    FreeListType<double, 800>,
    FreeListType<complex_data, sizeof(complex_data) * 100>,
    FreeListType<std::vector<int>, sizeof(std::vector<int>) * 100>>;

TYPED_TEST_CASE(FreeListTest, FreeListTestTypes);

////////////////////////////////////////////////////////////////////////////////

TYPED_TEST(FreeListTest, sizeof_) {

  FreeList<typename TestFixture::T, TestFixture::Size> freelist;

  EXPECT_EQ(TestFixture::Size, sizeof(freelist));
}

TYPED_TEST(FreeListTest, empty) {
  FreeList<typename TestFixture::T, TestFixture::Size> freelist;
  std::vector<typename TestFixture::T*> indexList;

  EXPECT_TRUE(this->freelist.empty());
  indexList.push_back(this->freelist.push());
  EXPECT_FALSE(this->freelist.empty());
  this->freelist.pop(indexList.back());
  indexList.pop_back();
  EXPECT_TRUE(this->freelist.empty());
}

TYPED_TEST(FreeListTest, full) {
  FreeList<typename TestFixture::T, TestFixture::Size> freelist;
  std::vector<typename TestFixture::T*> indexList;

  for (int i = 0; i < freelist.capacity(); ++i) {
    EXPECT_FALSE(freelist.full());
    indexList.push_back(freelist.push());
    ASSERT_NE(nullptr, indexList.back());
  }
  EXPECT_TRUE(freelist.full());

  freelist.pop(indexList.back());
  indexList.pop_back();
  EXPECT_FALSE(freelist.full());
}

TYPED_TEST(FreeListTest, size) {
  FreeList<typename TestFixture::T, TestFixture::Size> freelist;
  std::vector<typename TestFixture::T*> indexList;

  for (int i = 0; i < freelist.capacity(); ++i) {
    EXPECT_EQ(i, freelist.size());
    indexList.push_back(freelist.push());
    ASSERT_NE(nullptr, indexList.back());
  }
  EXPECT_EQ(freelist.capacity(), freelist.size());
}

TYPED_TEST(FreeListTest, capacity) {
  FreeList<typename TestFixture::T, TestFixture::Size> freelist;
  std::vector<typename TestFixture::T*> indexList;

  EXPECT_GT(TestFixture::Size / sizeof(typename TestFixture::T),
            freelist.capacity());
}

TYPED_TEST(FreeListTest, clear) {
  FreeList<typename TestFixture::T, TestFixture::Size> freelist;
  std::vector<typename TestFixture::T*> indexList;

  // TODO Check all items have destructors called
}

TYPED_TEST(FreeListTest, push) {
  FreeList<typename TestFixture::T, TestFixture::Size> freelist;
  std::vector<typename TestFixture::T*> indexList;

  for (int i = 0; i < freelist.capacity(); ++i) {
    indexList.push_back(freelist.push());
    ASSERT_NE(nullptr, indexList.back());

    // Check the pointers are within range
    EXPECT_GE(reinterpret_cast<uint8_t const*>(indexList.back()),
              reinterpret_cast<uint8_t const*>(&freelist));
    EXPECT_LT(reinterpret_cast<uint8_t const*>(indexList.back()),
              reinterpret_cast<uint8_t const*>(&freelist) + TestFixture::Size);
  }
  indexList.push_back(freelist.push());
  EXPECT_EQ(nullptr, indexList.back());
  indexList.pop_back();

  freelist.pop(indexList.back());
  indexList.pop_back();

  indexList.push_back(freelist.push());
  ASSERT_NE(nullptr, indexList.back());
}

TYPED_TEST(FreeListTest, push_with_args) {
  FreeList<typename TestFixture::T, TestFixture::Size> freelist;
  std::vector<typename TestFixture::T*> indexList;
  // TODO
}

TYPED_TEST(FreeListTest, data_integrity) {
  FreeList<typename TestFixture::T, TestFixture::Size> freelist;
  std::vector<typename TestFixture::T*> indexList;
  std::vector<typename TestFixture::T> valueList;

  for (int i = 0; i < freelist.capacity(); ++i) {
    indexList.push_back(freelist.push());
    valueList.push_back(this->value_store.next());
    *indexList.back() = valueList.back();
  }

  for (int i = 0; i < freelist.capacity(); ++i) {
    EXPECT_EQ(valueList[i], *indexList[i]);
  }
}

TYPED_TEST(FreeListTest, push_and_pop) {

  FreeList<typename TestFixture::T, TestFixture::Size> freelist;
  std::vector<typename TestFixture::T> valueList;

  // TODO change test to cover lists with smaller capacity

  if (freelist.capacity() >= 6) {
    typename TestFixture::T* d0 = freelist.push();
    valueList.push_back(this->value_store.next());
    *d0 = valueList.back();
    typename TestFixture::T* d1 = freelist.push();
    valueList.push_back(this->value_store.next());
    *d1 = valueList.back();
    typename TestFixture::T* d2 = freelist.push();
    valueList.push_back(this->value_store.next());
    *d2 = valueList.back();
    typename TestFixture::T* dm1 = freelist.push();
    *dm1 = this->value_store.next();
    typename TestFixture::T* dm2 = freelist.push();
    *dm2 = this->value_store.next();
    typename TestFixture::T* dm3 = freelist.push();
    *dm3 = this->value_store.next();

    freelist.pop(dm1);
    freelist.pop(dm2);

    typename TestFixture::T* d3 = freelist.push();
    valueList.push_back(this->value_store.next());
    *d3 = valueList.back();

    freelist.pop(dm3);

    typename TestFixture::T* dm4 = freelist.push();
    *dm3 = this->value_store.next();

    freelist.pop(dm4);

    EXPECT_EQ(valueList[0], *d0);
    EXPECT_EQ(valueList[1], *d1);
    EXPECT_EQ(valueList[2], *d2);
    EXPECT_EQ(valueList[3], *d3);

    freelist.pop(d0);
    freelist.pop(d1);
    freelist.pop(d2);
    freelist.pop(d3);
  }
}

// TODO Thread test

// TODO Check if virtual classes without derived data members can be stored


