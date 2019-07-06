//
// Created by Daniel Jones on 23.06.19.
//

#ifndef QUADTREE_FREELIST_H
#define QUADTREE_FREELIST_H

#include <atomic>
#include <cassert>
#include <cstdint>
#include <type_traits>

/**
 * Note: there is no empty() function implemented, because a constant-time O(1)
 * implementation is not possible.
 *
 * @tparam T Data type to store.
 * @tparam Size Total number of bytes for this FreeList: sizeof(*this) == Size.
 * @tparam IndexSize Number of bytes to use for indices, default 2
 */
template<typename T, uint32_t Size, uint8_t IndexSize = 2>
class FreeList {
 public:
  static_assert(std::is_pod<T>::value, "Stored data type must be POD");

  using index_type = uint16_t;
  using value_type = T;
  using byte = uint8_t;

  /**
   *
   */
  static constexpr uint32_t
      element_size = sizeof(T) > IndexSize ? sizeof(T) : IndexSize;

  /**
   * Static constant defined the maximum capacity of FreeLists of this type.
   */
  static constexpr index_type capacity = (Size - 2 * IndexSize) / element_size;

  inline FreeList();
  inline ~FreeList();

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
  union _meta {

    static constexpr index_type buffer_elements =
        element_size >= (2*IndexSize) ? 1
                                      : ((2*IndexSize + element_size) / element_size);
    uint8_t _buffer[buffer_elements * element_size];

    struct _detail {
      std::atomic<index_type> next;
      std::atomic<index_type> free;
    } detail;
  } meta;
  byte data[capacity * element_size];

};

template<typename T, uint32_t Size, uint8_t IndexSize>
constexpr uint16_t FreeList<T, Size, IndexSize>::capacity;

template<typename T, uint32_t Size, uint8_t IndexSize>
FreeList<T, Size, IndexSize>::FreeList() {
  meta.detail.next = _meta::buffer_elements;
  meta.detail.free = 0;
}

template<typename T, uint32_t Size, uint8_t IndexSize>
FreeList<T, Size, IndexSize>::~FreeList() {}

template<typename T, uint32_t Size, uint8_t IndexSize>
T const* FreeList<T, Size, IndexSize>::get(index_type index) const {
  assert(index < capacity);
  return reinterpret_cast<T const*>(data + (index * element_size));
}

template<typename T, uint32_t Size, uint8_t IndexSize>
T* FreeList<T, Size, IndexSize>::get(index_type index) {
  assert(index < capacity);
  return reinterpret_cast<T*>(data + (index * element_size));
}

template<typename T, uint32_t Size, uint8_t IndexSize>
typename FreeList<T, Size, IndexSize>::index_type
FreeList<T, Size, IndexSize>::index(T const* item) const {

  index_type index = (reinterpret_cast<byte const*>(item)
      - reinterpret_cast<byte const*>(this)) / element_size;
  assert(reinterpret_cast<byte const* >(item)
             >= reinterpret_cast<byte const*>(this));
  assert((reinterpret_cast<byte const*>(item)
      - reinterpret_cast<byte const*>(this))
             % element_size == 0);
  assert(index < capacity);
  return index;

}

template<typename T, uint32_t Size, uint8_t IndexSize>
T* FreeList<T, Size, IndexSize>::push() {
  // Read the free index
  index_type index = meta.detail.free;
  // If it is not zero, then there is a previously-freed item
  if (index != 0) {
    // Change free to the data stored at that index
    meta.detail.free = *reinterpret_cast<index_type*>(data + (index * element_size));
    // Return the previously freed item
    return get(index);
  } else {
    // No previously-freed items
    if (meta.detail.next == capacity) {
      // No next items
      return nullptr;
    }
    // Return the next item
    return get(meta.detail.next++);
  }

}

template<typename T, uint32_t Size, uint8_t IndexSize>
void FreeList<T, Size, IndexSize>::pop(T* item) {
  pop(index(item));
};

template<typename T, uint32_t Size, uint8_t IndexSize>
void FreeList<T, Size, IndexSize>::pop(index_type index) {
  assert(index < capacity);

  index_type old_free = meta.detail.free;
  meta.detail.free = index;
  *reinterpret_cast<uint16_t*>(data + (index * element_size)) = old_free;
}

template<typename T, uint32_t Size, uint8_t IndexSize>
bool FreeList<T, Size, IndexSize>::full() const {
  return meta.detail.next >= capacity && meta.detail.free == capacity;
}

#endif //QUADTREE_FREELIST_H
