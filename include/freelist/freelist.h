//
// Created by Daniel Jones on 23.06.19.
//

#ifndef QUADTREE_FREELIST_H
#define QUADTREE_FREELIST_H

#include <atomic>
#include <cassert>
#include <cstdint>
#include <limits>
#include <type_traits>

/**
 * Note: there is no empty() function implemented, because a constant-time O(1)
 * implementation is not possible.
 *
 * @tparam T Data type to store.
 * @tparam Size Total number of bytes for this FreeList: sizeof(*this) == Size.
 */
template<typename T, uint64_t Size>
class FreeList {
 public:
  static_assert(std::is_pod<T>::value, "Stored data type must be POD");
  static_assert(Size % sizeof(T) == 0, "Size must be a multiple of sizeof(T)");

  /**
   * Forwarding the stored type.
   */
  using value_type = T;

  /**
   * Type used for storing indexes.
   * Either uint8_t, uint16_t, uint32_t or uint64_t depending on required size.
   */
  using index_type = typename std::conditional<
      Size <= std::numeric_limits<uint8_t>::max(),
      uint8_t,
      typename std::conditional<
          (Size / 2) <= std::numeric_limits<uint16_t>::max(),
          uint16_t,
          typename std::conditional<
              (Size / 4) <= std::numeric_limits<uint32_t>::max(),
              uint32_t,
              uint64_t>::type>::type>::type;

 private:
  union _element {
    T data;
    index_type index;
  };

  struct _overhead {
    std::atomic<index_type> next;
    std::atomic<index_type> free;
    //std::atomic<index_type> count; // TODO
  };

 public:

  /**
   *
   */
  static constexpr uint64_t element_size = sizeof(_element);

  static constexpr index_type index_count = Size / element_size;

  static constexpr index_type element_overhead_count =
      (sizeof(_overhead) + element_size - 1) / element_size;

  /**
   * Static constant defined the maximum capacity of FreeLists of this type.
   */
  static constexpr index_type capacity = index_count - element_overhead_count;

  inline FreeList();
  inline ~FreeList();

  inline void clear();

  inline T* get(index_type index);
  inline T const* get(index_type index) const;
  inline index_type index(T const* item) const;

  inline T* push();
  inline void pop(T* item);
  inline void pop(index_type index);

  /**
   * Check if the FreeList is full.
   * @return True if the FreeList is full.
   */
  inline bool full() const;

 private:
  union _data {
    _element elements[index_count];
    _overhead overhead;
  } data;
};

template<typename T, uint64_t Size>
constexpr typename FreeList<T, Size>::index_type FreeList<T, Size>::capacity;

template<typename T, uint64_t Size>
FreeList<T, Size>::FreeList() {
  data.overhead.next = element_overhead_count;
  data.overhead.free = 0;
}

template<typename T, uint64_t Size>
FreeList<T, Size>::~FreeList() {}

template<typename T, uint64_t Size>
void FreeList<T, Size>::clear() {
  // TODO
}

template<typename T, uint64_t Size>
T const* FreeList<T, Size>::get(index_type index) const {
  assert(index < index_count);
  return &data.elements[index].data;
}

template<typename T, uint64_t Size>
T* FreeList<T, Size>::get(index_type index) {
  assert(index < index_count);
  return &data.elements[index].data;
}

template<typename T, uint64_t Size>
typename FreeList<T, Size>::index_type
FreeList<T, Size>::index(T const* item) const {
  assert(item);
  assert(reinterpret_cast<void const* >(item)
             >= reinterpret_cast<void const*>(this));
  assert((reinterpret_cast<uint8_t const*>(item)
      - reinterpret_cast<uint8_t const*>(this))
             % element_size == 0);
  index_type index = reinterpret_cast<_element const*>(item) - data.elements;

  assert(index < index_count);
  return index;

}

template<typename T, uint64_t Size>
T* FreeList<T, Size>::push() {
  // Read the free index
  index_type index = data.overhead.free;
  // If it is not zero, then there is a previously-freed item
  if (index) {
    // Change free to the data stored at that index
    data.overhead.free = *reinterpret_cast<index_type*>(get(index));
    // Return the previously freed item
    return get(index);
  } else {
    // No previously-freed items
    if (data.overhead.next == index_count) {
      // No next items
      return nullptr;
    }
    // Return the next item
    return get(data.overhead.next++);
  }

}

template<typename T, uint64_t Size>
void FreeList<T, Size>::pop(T* item) {
  pop(index(item));
};

template<typename T, uint64_t Size>
void FreeList<T, Size>::pop(index_type index) {
  assert(index < index_count);

  index_type old_free = data.overhead.free;
  data.overhead.free = index;
  *reinterpret_cast<index_type*>(get(index)) = old_free;
}

template<typename T, uint64_t Size>
bool FreeList<T, Size>::full() const {
  return data.overhead.next == index_count && data.overhead.free == 0;
}

#endif //QUADTREE_FREELIST_H
