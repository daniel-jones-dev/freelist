//
// Created by Daniel Jones on 23.06.19.
//

#include <freelist/freelist.h>

#include <map>

#include <gtest/gtest.h>


// TODO Thread-safety test

// TODO Check if virtual classes without derived data members can be stored

// TODO push() with args

////////////////////////////////////////////////////////////////////////////////

template<typename T>
struct TypeTraits {
  static const T zero;
  static void inc(T& t) { ++t; }
};
template<typename T>
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

  using this_FreeList = freelist<typename _T::T, _T::size>;
  using T = typename _T::T;
  static constexpr uint64_t Size = _T::size;

  this_FreeList fl;
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

template<>
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

template<>
template<typename T>
struct TypeTraits<std::vector<T>> {
  static const std::vector<T> zero;
  static void inc(std::vector<T>& t) { t.push_back(T(t.size())); }
};
template<typename T>
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

  freelist<typename TestFixture::T, TestFixture::Size> fl;

  EXPECT_EQ(TestFixture::Size, sizeof(fl));
}

TYPED_TEST(FreeListTest, empty) {
  freelist<typename TestFixture::T, TestFixture::Size> fl;
  std::vector<typename TestFixture::T*> indexList;

  EXPECT_TRUE(this->fl.empty());
  indexList.push_back(this->fl.push());
  EXPECT_FALSE(this->fl.empty());
  this->fl.pop(indexList.back());
  indexList.pop_back();
  EXPECT_TRUE(this->fl.empty());
}

TYPED_TEST(FreeListTest, full) {
  freelist<typename TestFixture::T, TestFixture::Size> fl;
  std::vector<typename TestFixture::T*> indexList;

  for (int i = 0; i < fl.capacity(); ++i) {
    EXPECT_FALSE(fl.full());
    indexList.push_back(fl.push());
    ASSERT_NE(nullptr, indexList.back());
  }
  EXPECT_TRUE(fl.full());

  fl.pop(indexList.back());
  indexList.pop_back();
  EXPECT_FALSE(fl.full());
}

TYPED_TEST(FreeListTest, size) {
  freelist<typename TestFixture::T, TestFixture::Size> fl;
  std::vector<typename TestFixture::T*> indexList;

  for (int i = 0; i < fl.capacity(); ++i) {
    EXPECT_EQ(i, fl.size());
    indexList.push_back(fl.push());
    ASSERT_NE(nullptr, indexList.back());
  }
  EXPECT_EQ(fl.capacity(), fl.size());
}

TYPED_TEST(FreeListTest, capacity) {
  freelist<typename TestFixture::T, TestFixture::Size> fl;
  std::vector<typename TestFixture::T*> indexList;

  EXPECT_GT(TestFixture::Size / sizeof(typename TestFixture::T),
            fl.capacity());
}

TYPED_TEST(FreeListTest, clear) {
  freelist<typename TestFixture::T, TestFixture::Size> fl;
  std::vector<typename TestFixture::T*> indexList;

  // TODO
}

TYPED_TEST(FreeListTest, push) {
  freelist<typename TestFixture::T, TestFixture::Size> fl;
  std::vector<typename TestFixture::T*> indexList;

  for (int i = 0; i < fl.capacity(); ++i) {
    indexList.push_back(fl.push());
    ASSERT_NE(nullptr, indexList.back());

    // Check the pointers are within range
    EXPECT_GE(reinterpret_cast<uint8_t const*>(indexList.back()),
              reinterpret_cast<uint8_t const*>(&fl));
    EXPECT_LT(reinterpret_cast<uint8_t const*>(indexList.back()),
              reinterpret_cast<uint8_t const*>(&fl) + TestFixture::Size);
  }
  indexList.push_back(fl.push());
  EXPECT_EQ(nullptr, indexList.back());
  indexList.pop_back();

  fl.pop(indexList.back());
  indexList.pop_back();

  indexList.push_back(fl.push());
  ASSERT_NE(nullptr, indexList.back());
}

TYPED_TEST(FreeListTest, data_integrity) {
  freelist<typename TestFixture::T, TestFixture::Size> fl;
  std::vector<typename TestFixture::T*> indexList;
  std::vector<typename TestFixture::T> valueList;

  for (int i = 0; i < fl.capacity(); ++i) {
    indexList.push_back(fl.push());
    valueList.push_back(this->value_store.next());
    *indexList.back() = valueList.back();
  }

  for (int i = 0; i < fl.capacity(); ++i) {
    EXPECT_EQ(valueList[i], *indexList[i]);
  }
}

TYPED_TEST(FreeListTest, push_and_pop) {

  freelist<typename TestFixture::T, TestFixture::Size> fl;
  std::vector<typename TestFixture::T> valueList;

  // TODO change test to cover lists with smaller capacity

  if (fl.capacity() >= 6) {
    typename TestFixture::T* d0 = fl.push();
    valueList.push_back(this->value_store.next());
    *d0 = valueList.back();
    typename TestFixture::T* d1 = fl.push();
    valueList.push_back(this->value_store.next());
    *d1 = valueList.back();
    typename TestFixture::T* d2 = fl.push();
    valueList.push_back(this->value_store.next());
    *d2 = valueList.back();
    typename TestFixture::T* dm1 = fl.push();
    *dm1 = this->value_store.next();
    typename TestFixture::T* dm2 = fl.push();
    *dm2 = this->value_store.next();
    typename TestFixture::T* dm3 = fl.push();
    *dm3 = this->value_store.next();

    fl.pop(dm1);
    fl.pop(dm2);

    typename TestFixture::T* d3 = fl.push();
    valueList.push_back(this->value_store.next());
    *d3 = valueList.back();

    fl.pop(dm3);

    typename TestFixture::T* dm4 = fl.push();
    *dm3 = this->value_store.next();

    fl.pop(dm4);

    EXPECT_EQ(valueList[0], *d0);
    EXPECT_EQ(valueList[1], *d1);
    EXPECT_EQ(valueList[2], *d2);
    EXPECT_EQ(valueList[3], *d3);

    fl.pop(d0);
    fl.pop(d1);
    fl.pop(d2);
    fl.pop(d3);
  }
}

////////////////////////////////////////////////////////////////////////////////

// Tests to ensure that all objects are deleted

class FreeListDestructorTest : public ::testing::Test {
 public:
  using PointerConstructorCounter = std::map<void*, int32_t>;
  static PointerConstructorCounter counts;

 protected:
  void SetUp() override {
    counts.clear();
  }

  void TearDown() override {

    for (auto kv : counts) {
      EXPECT_LE(0, kv.second) << "Too many destructors called on " << kv.first;
      EXPECT_GE(0, kv.second) << "Too few destructors called on  " << kv.first;
    }
  }

  class InstanceCounter {
   public:
    InstanceCounter() {
      void* ptr = this;
      if (FreeListDestructorTest::counts.count(ptr) == 0)
        FreeListDestructorTest::counts[ptr] = 0;
      ++FreeListDestructorTest::counts[ptr];
    }

    ~InstanceCounter() {
      void* ptr = this;
      --FreeListDestructorTest::counts[ptr];
    }
  };
};
FreeListDestructorTest::PointerConstructorCounter
    FreeListDestructorTest::counts{};

TEST_F(FreeListDestructorTest, empty) {
  freelist<InstanceCounter, 100> fl;
}

TEST_F(FreeListDestructorTest, push_and_pop) {
  freelist<InstanceCounter, 100> fl;
  auto p1 = fl.push();
  auto p2 = fl.push();
  auto p3 = fl.push();
  fl.pop(p1);
  auto p4 = fl.push();
  auto p5 = fl.push();
  fl.pop(p2);
  fl.pop(p4);
  fl.pop(p5);
  auto p6 = fl.push();
  fl.pop(p3);
  fl.pop(p6);
}

TEST_F(FreeListDestructorTest, missing_pop) {
  freelist<InstanceCounter, 100> fl;
  auto p1 = fl.push();
  fl.push();
  auto p3 = fl.push();
  fl.pop(p1);
  fl.push();
  fl.push();
  auto p6 = fl.push();
  fl.pop(p3);
  fl.pop(p6);
}

////////////////////////////////////////////////////////////////////////////////
