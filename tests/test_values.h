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
// Created by Daniel Jones on 14.07.19.
//

#ifndef TESTS_TEST_VALUES_H_
#define TESTS_TEST_VALUES_H_

#include <cstdint>
#include <string>
#include <vector>

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

template <>
template <typename T>
struct TypeTraits<std::vector<T>> {
  static const std::vector<T> zero;
  static void inc(std::vector<T>& t) { t.push_back(T(t.size())); }
};
template <typename T>
const std::vector<T> TypeTraits<std::vector<T>>::zero{};

////////////////////////////////////////////////////////////////////////////////

template <>
struct TypeTraits<std::string> {
  static const std::string zero;
  static void inc(std::string& t) {
    try {
      int64_t v = stoll(t);
      std::stringstream ss;
      ss << ++v;
      t = ss.str();
    } catch (std::invalid_argument&) {
      t = "0";
    }
  }
};

const std::string TypeTraits<std::string>::zero{""};

////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////

template <int Size = 7>
struct AbnormalSize {
  char data[Size];
};

template <>
template <int Size>
struct TypeTraits<AbnormalSize<Size>> {
  static const AbnormalSize<Size> zero;
  static void inc(AbnormalSize<Size>& t) {
    for (int i = 0; i < Size; ++i) {
      ++t.data[i];
      if (t.data[i]) break;
    }
  }
};

template <int Size>
const AbnormalSize<Size> TypeTraits<AbnormalSize<Size>>::zero{0};

template <int Size>
bool operator==(AbnormalSize<Size> const& l, AbnormalSize<Size> const& r) {
  for (int i = 0; i < Size; ++i)
    if (l.data[i] != r.data[i]) return false;
  return true;
}

////////////////////////////////////////////////////////////////////////////////

#endif  // TESTS_TEST_VALUES_H_
