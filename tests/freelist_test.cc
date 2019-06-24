//
// Created by Daniel Jones on 23.06.19.
//

#include <freelist/freelist.h>

#include <gtest/gtest.h>

struct data {
  double d;
};

TEST(freelist, sizeof_) {

  FreeList<data, 8000> fl;
  EXPECT_EQ(8000, sizeof(fl));
}

TEST(freelist, empty) {
  std::vector<data*> indexList;
  FreeList<data, 800> fl;
  EXPECT_TRUE(fl.empty());
  indexList.push_back(fl.push());
  EXPECT_FALSE(fl.empty());
  fl.pop(indexList.back());
  indexList.pop_back();
  EXPECT_TRUE(fl.empty());
}

TEST(freelist, full) {
  std::vector<data*> indexList;
  FreeList<data, 16> fl;
  for (int i = 0; i < fl.capacity(); ++i) {
    EXPECT_FALSE(fl.full());
    indexList.push_back(fl.push());
    ASSERT_NE(nullptr, indexList.back());
    EXPECT_GE(reinterpret_cast<uint8_t const*>(indexList.back()),
              reinterpret_cast<uint8_t const*>(&fl));
    EXPECT_LT(reinterpret_cast<uint8_t const*>(indexList.back()),
              reinterpret_cast<uint8_t const*>(&fl) + 16);
  }
  EXPECT_TRUE(fl.full());

  fl.pop(indexList.back());
  indexList.pop_back();
  EXPECT_FALSE(fl.full());
}

TEST(freelist, size) {
  std::vector<data*> indexList;
  FreeList<data, 8000> fl;

  for (int i = 0; i < fl.capacity(); ++i) {
    EXPECT_EQ(i, fl.size());
    indexList.push_back(fl.push());
    ASSERT_NE(nullptr, indexList.back());
  }
  EXPECT_EQ(fl.capacity(), fl.size());
}

TEST(freelist, capacity) {
  FreeList<data, 8000> fl;
  EXPECT_EQ(999, fl.capacity());
}

TEST(freelist, clear) {
 // TODO Check all items have destructors called
}

TEST(freelist, push) {
  std::vector<data*> indexList;
  FreeList<data, 800> fl;
  for (int i = 0; i < fl.capacity(); ++i) {
    indexList.push_back(fl.push());
    ASSERT_NE(nullptr, indexList.back());
  }
  indexList.push_back(fl.push());
  EXPECT_EQ(nullptr, indexList.back());
  indexList.pop_back();

  fl.pop(indexList.back());
  indexList.pop_back();

  indexList.push_back(fl.push());
  ASSERT_NE(nullptr, indexList.back());
}

TEST(freelist, push_with_args) {
  // TODO
}

TEST(freelist, data_integrity) {
  std::vector<data*> indexList;
  FreeList<data, 800> fl;

  for (int i = 0; i < fl.capacity(); ++i) {
    indexList.push_back(fl.push());
    indexList.back()->d = double(i);
  }

  for (int i = 0; i < fl.capacity(); ++i) {
    EXPECT_FLOAT_EQ(double(i), indexList[i]->d);
  }
}

TEST(freelist, push_and_pop) {

  std::vector<data*> indexList;
  FreeList<data, 800> fl;

  data* d0 = fl.push();
  d0->d = 0.0;
  data* d1 = fl.push();
  d1->d = 1.0;
  data* d2 = fl.push();
  d2->d = 2.0;
  data* dm1 = fl.push();
  dm1->d = -1.0;
  data* dm2 = fl.push();
  dm2->d = -2.0;
  data* dm3 = fl.push();
  dm3->d = -3.0;

  fl.pop(dm1);
  fl.pop(dm2);

  data* d3 = fl.push();
  d3->d = 3.0;

  fl.pop(dm3);

  data* dm4 = fl.push();
  dm4->d = -4.0;

  fl.pop(dm4);

  EXPECT_EQ(0.0, d0->d);
  EXPECT_EQ(1.0, d1->d);
  EXPECT_EQ(2.0, d2->d);
  EXPECT_EQ(3.0, d3->d);

  fl.pop(d0);
  fl.pop(d1);
  fl.pop(d2);
  fl.pop(d3);

}

// TODO Thread test
