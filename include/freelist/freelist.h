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
 * FreeList allows random allocation and deletion of items of a fixed type,
 * without the need for dynamic memory allocation.
 *
 * FreeList is defined by the type to be stored and the number of bytes that
 * the freelist should occupy.
 *
 * FreeList supports empty constructors, constructors with arguments, and
 * destructors.
 *
 * @tparam T Data type to store.
 * @tparam Size Total number of bytes for this FreeList: sizeof(*this) == Size.
 */
template<typename T, uint64_t Size>
class freelist {
 public:
  static_assert(!std::is_abstract<T>::value,
                "Stored data type must not be abstract");
  static_assert(Size % sizeof(T) == 0, "Size must be a multiple of sizeof(T)");

  /**
   * Type that is stored in each active element.
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

  /**
   * Construct an empty freelist.
   */
  inline freelist();

  /**
   * Destructor, calls delete on all existing items.
   */
  inline ~freelist();

  /**
   * Check if the freelist is empty.
   * @return True if the freelist contains no items.
   */
  inline bool empty() const;

  /**
   * Check if the freelist is full.
   * @return True if the freelist is full.
   */
  inline bool full() const;

  /**
   * Check the number of active items in the freelist.
   * @return Number of active items in the freelist.
   */
  inline index_type size() const;

  /**
   * Get the maximum number of items that can be stored.
   * @return Maximum number of stored items.
   */
  inline static index_type capacity();

  /**
   * Removes all items from the list, calling destructor for each.
   */
  inline void clear();

  /**
   * Allocates a new item in the freelist, and calls empty constructor.
   * @return Pointer to new item, or nullptr if full.
   */
  inline T* push();

  /**
   * Allocates a new item in the freelist, and calls non-empty constructor.
   * @return Pointer to new item, or nullptr if full.
   */
  template<typename... Args>
  inline T* push(Args... args);

  /**
   * Deletes an item from the freelist, and calls destructor.
   * @param item Pointer to item to delete.
   */
  inline void pop(T* item);

  /**
   * Creates new item in the freelist, does not call constructor.
   * @return Index of new item, or 0 if full.
   */
  inline index_type push_index();

  /**
   * Removes item at specified index, does not call destructor.
   * @param index Index of item to remove.
   */
  inline void pop_index(index_type index);

  /**
   * Get the index of the specified item.
   * @param item Pointer of item to find.
   * @return Index of the specified item.
   */
  inline index_type index(T const* item) const;

  /**
   * Convert an index into a pointer to the corresponding item.
   * @param index Index of item to retrieve.
   * @return Pointer to item.
   */
  inline T* get(index_type index);

  /**
   * Convert an index into a pointer to the corresponding item, const version.
   * @param index Index of item to retrieve.
   * @return Pointer to item.
   */
  inline T const* get(index_type index) const;

 private:
  union _element {
    // Empty constructor and destructor - deletion will be handled manually
    _element() {}
    ~_element() {}

    T data;
    index_type index;
  };

  struct _overhead {
    std::atomic<index_type> next;
    std::atomic<index_type> free;
    std::atomic<index_type> count;
  };

  static constexpr uint64_t element_size = sizeof(_element);

  static constexpr index_type index_count = Size / element_size;

  static constexpr index_type element_overhead_count =
      (sizeof(_overhead) + element_size - 1) / element_size;

  static_assert(index_count > element_overhead_count,
                "freelist is too small to contain elements");



  union _data {
    // Empty constructor and destructor - deletion will be handled manually
    _data() {}
    ~_data() {}

    _element elements[index_count];
    _overhead overhead;
  } data;
};

template<typename T, uint64_t Size>
freelist<T, Size>::freelist() {
  data.overhead.free = 0;
  data.overhead.count = 0;
  data.overhead.next = element_overhead_count;
}

template<typename T, uint64_t Size>
freelist<T, Size>::~freelist() {
  clear();
}

template<typename T, uint64_t Size>
bool freelist<T, Size>::empty() const {
  return data.overhead.count.load() == 0;
}

template<typename T, uint64_t Size>
bool freelist<T, Size>::full() const {
  return data.overhead.next == index_count && data.overhead.free == 0;
}

template<typename T, uint64_t Size>
typename freelist<T, Size>::index_type freelist<T, Size>::size() const {
  return data.overhead.count.load();
}

template<typename T, uint64_t Size>
typename freelist<T, Size>::index_type freelist<T, Size>::capacity() {
  return index_count - element_overhead_count;
}

template<typename T, uint64_t Size>
void freelist<T, Size>::clear() {

  // Traverse the free list to find already-freed items
  bool free[index_count];
  for (index_type i = element_overhead_count; i < index_count; ++i) {
    free[i] = false;
  }

  index_type curr_free = data.overhead.free.load(std::memory_order_relaxed);
  while (curr_free != 0) {
    free[curr_free] = true;
    curr_free = data.elements[curr_free].index;
  };

  // Delete all non-freed items up to next
  const index_type next = data.overhead.next.load(std::memory_order_relaxed);
  for (index_type i = element_overhead_count; i < next; ++i) {
    if (!free[i]) {
      // Call destructor
      data.elements[i].data.~T();
    }
  }

  data.overhead.free = 0;
  data.overhead.count = 0;
  data.overhead.next = element_overhead_count;
}

template<typename T, uint64_t Size>
T* freelist<T, Size>::push() {
  index_type index = push_index();
  if (!index) {
    return nullptr;
  }

  // placement-new the item
  return new(get(index)) T();
}

template<typename T, uint64_t Size>
template<typename... Args>
T* freelist<T, Size>::push(Args... args) {
  index_type index = push_index();
  if (!index) {
    return nullptr;
  }

  // placement-new the item
  return new(get(index)) T(args...);
}

template<typename T, uint64_t Size>
typename freelist<T, Size>::index_type freelist<T, Size>::push_index() {
  // Read the free index
  index_type index = data.overhead.free;
  // If it is not zero, then there is a previously-freed item
  if (index) {
    // Change free to the data stored at that index
    data.overhead.free = *reinterpret_cast<index_type*>(get(index));
    ++data.overhead.count;

    // Return the previously freed item
    return index;
  } else {
    // No previously-freed items
    if (data.overhead.next == index_count) {
      // No next items
      return 0;
    }
    ++data.overhead.count;

    // Return the next item
    return data.overhead.next++;
  }
}

template<typename T, uint64_t Size>
void freelist<T, Size>::pop(T* item) {
  assert(item);

  // placement-delete the item
  item->~T();
  pop_index(index(item));
};

template<typename T, uint64_t Size>
void freelist<T, Size>::pop_index(index_type index) {
  assert(index >= element_overhead_count);
  assert(index < index_count);

  index_type old_free = data.overhead.free;
  data.overhead.free = index;
  *reinterpret_cast<index_type*>(get(index)) = old_free;
  --data.overhead.count;
}

template<typename T, uint64_t Size>
T const* freelist<T, Size>::get(index_type index) const {
  assert(index >= element_overhead_count);
  assert(index < index_count);
  return &data.elements[index].data;
}

template<typename T, uint64_t Size>
T* freelist<T, Size>::get(index_type index) {
  assert(index >= element_overhead_count);
  assert(index < index_count);
  return &data.elements[index].data;
}

template<typename T, uint64_t Size>
typename freelist<T, Size>::index_type
freelist<T, Size>::index(T const* item) const {
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

#endif //QUADTREE_FREELIST_H
