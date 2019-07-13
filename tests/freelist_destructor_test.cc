//
// Copyright 2019 Daniel Jones
//
// Created by Daniel Jones on 06.07.19.
//

#include <freelist/freelist.h>

#include <map>

#include <gtest/gtest.h>

////////////////////////////////////////////////////////////////////////////////

// Tests to ensure that all objects are deleted

class FreeListDestructorTest : public ::testing::Test {
 public:
  using PointerConstructorCounter = std::map<void*, int32_t>;
  static PointerConstructorCounter counts;

 protected:
  void SetUp() override { counts.clear(); }

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
  freelist::FreeList<InstanceCounter, 100> fl;
}

TEST_F(FreeListDestructorTest, push_and_pop) {
  freelist::FreeList<InstanceCounter, 100> fl;
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
  freelist::FreeList<InstanceCounter, 100> fl;
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

TEST_F(FreeListDestructorTest, early_clear) {
  freelist::FreeList<InstanceCounter, 100> fl;
  auto p1 = fl.push();
  fl.push();
  auto p3 = fl.push();
  fl.pop(p1);
  fl.push();
  fl.push();
  auto p6 = fl.push();

  fl.clear();
}

TEST_F(FreeListDestructorTest, make_unique) {
  freelist::FreeList<InstanceCounter, 100> fl;
  {
    auto p1 = fl.make_unique();
    fl.make_unique();
    auto p3 = fl.make_unique();
    p1.reset();
    fl.make_unique();
    fl.make_unique();
    auto p6 = fl.make_unique();
  }
}

TEST_F(FreeListDestructorTest, make_shared) {
  freelist::FreeList<InstanceCounter, 100> fl;
  {
    auto p1 = fl.make_shared();
    fl.make_shared();
    auto p3 = fl.make_shared();
    p1.reset();
    fl.make_shared();
    fl.make_shared();
    auto p6 = fl.make_shared();
  }
}
