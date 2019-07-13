//
// Copyright 2019 Daniel Jones
//
// Created by Daniel Jones on 23.06.19.
//

#ifndef INCLUDE_FREELIST_FREELIST_H_
#define INCLUDE_FREELIST_FREELIST_H_

#include <atomic>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>

namespace freelist {

/**
 * FreeList allows random allocation and deletion of items of a fixed type,
 * without the need for dynamic memory allocation.
 *
 * FreeList is defined by the type to be stored and the number of bytes that
 * the FreeList should occupy.
 *
 * The alloc function constructs new items passing provided arguments to any
 * constructor. The free function destructs items.
 *
 * @tparam T Data type to store.
 * @tparam Size Total number of bytes for this FreeList: sizeof(*this) == Size.
 */
template <typename T, uint64_t Size>
class FreeList {
 public:
  static_assert(!std::is_abstract<T>::value,
                "Stored data type must not be abstract");
  static_assert(Size % sizeof(T) == 0, "Size must be a multiple of sizeof(T)");

  /**
   * Type that is stored in each active element.
   */
  using value_type = T;

  /**
   * Type used for storing indexes. Set to an unsigned integer large enough to
   * store all indexes.
   */
  using index_type = typename std::conditional<
      Size <= std::numeric_limits<uint8_t>::max(), uint8_t,
      typename std::conditional<
          (Size / 2) <= std::numeric_limits<uint16_t>::max(), uint16_t,
          typename std::conditional<(Size / 4) <=
                                        std::numeric_limits<uint32_t>::max(),
                                    uint32_t, uint64_t>::type>::type>::type;

  /**
   * Type of function used to free items.
   */
  using Deleter = std::function<void(T*)>;

  /**
   * Type of std::unique_ptr returned by make_unique.
   */
  using UniquePtr = std::unique_ptr<T, Deleter>;

  /**
   * Construct an empty FreeList.
   */
  inline FreeList();

  /**
   * Destructor, calls delete on all existing items.
   */
  inline ~FreeList() noexcept;

  /**
   * Check if the FreeList is empty.
   * @return True if the FreeList contains no items.
   */
  inline bool empty() const noexcept;

  /**
   * Check if the FreeList is full.
   * @return True if the FreeList is full.
   */
  inline bool full() const noexcept;

  /**
   * Check the number of active items in the FreeList.
   * @return Number of active items in the FreeList.
   */
  inline index_type size() const noexcept;

  /**
   * Get the maximum number of items that can be stored.
   * @return Maximum number of stored items.
   */
  inline static index_type max_size() noexcept;

  /**
   * Synonym for max_size()
   */
  inline static index_type capacity() noexcept { return max_size(); }

  /**
   * Removes all items from the list, calling destructor for each.
   */
  inline void clear() noexcept;

  /**
   * Allocates a new item in the FreeList, and calls constructor.
   * @tparam Args Type of arguments for item constructor.
   * @param args Arguments to provide to item constructor.
   * @return Pointer to new item.
   * @exception std::bad_alloc If the freelist is full.
   * @exception any Exceptions thrown by the constructor are forwarded.
   * If any exception is thrown, the freelist state will be unchanged.
   */
  template <typename... Args>
  inline T* alloc(Args... args);

  /**
   * Synonym for alloc().
   */
  template <typename... Args>
  inline T* push(Args... args) {
    return alloc(args...);
  }

  /**
   * Constructs item in the Freelist with arguments, and returns unique_ptr.
   * @tparam Args Type of arguments for item constructor.
   * @param args Arguments to provide to item constructor.
   * @return unique_ptr to the item.
   * @exception std::bad_alloc If the freelist is full.
   * @exception any Exceptions thrown by the constructor are forwarded.
   * If any exception is thrown, the freelist state will be unchanged.
   */
  template <typename... Args>
  inline UniquePtr make_unique(Args... args);

  /**
   * Constructs item in the Freelist with arguments, and returns shared_ptr.
   * @tparam Args Type of arguments for item constructor.
   * @param args Arguments to provide to item constructor.
   * @return shared_ptr to the item.
   * @exception std::bad_alloc If the freelist is full.
   * @exception any Exceptions thrown by the constructor are forwarded.
   * If any exception is thrown, the freelist state will be unchanged.
   */
  template <typename... Args>
  inline std::shared_ptr<T> make_shared(Args... args);

  /**
   * Deletes an item from the FreeList, and calls destructor.
   * @param item Pointer to item to delete.
   */
  inline void free(T* item) noexcept;

  /**
   * Synonym for free().
   */
  inline void pop(T* item) noexcept { free(item); }

  /**
   * Creates new item in the FreeList, does not call constructor.
   * @return Index of new item, or 0 if full.
   */
  inline index_type push_index() noexcept;

  /**
   * Removes item at specified index, does not call destructor.
   * @param index Index of item to remove.
   */
  inline void pop_index(index_type index) noexcept;

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

  /**
   * Returns a function to delete items from this freelist.
   * @return Deleter function.
   */
  inline Deleter deleter() {
    return std::bind(&FreeList::free, this, std::placeholders::_1);
  }

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

  static constexpr uint64_t kElementSize = sizeof(_element);

  static constexpr index_type kIndexCount = Size / kElementSize;

  static constexpr index_type kElementOverheadCount =
      (sizeof(_overhead) + kElementSize - 1) / kElementSize;

  static_assert(kIndexCount > kElementOverheadCount,
                "FreeList is too small to contain elements");

  union _data {
    // Empty constructor and destructor - deletion will be handled manually
    _data() {}
    ~_data() {}

    _element elements[kIndexCount];
    _overhead overhead;
  } data;
};

template <typename T, uint64_t Size>
FreeList<T, Size>::FreeList() {
  data.overhead.free = 0;
  data.overhead.count = 0;
  data.overhead.next = kElementOverheadCount;
}

template <typename T, uint64_t Size>
FreeList<T, Size>::~FreeList() noexcept {
  clear();
}

template <typename T, uint64_t Size>
bool FreeList<T, Size>::empty() const noexcept {
  return data.overhead.count.load() == 0;
}

template <typename T, uint64_t Size>
bool FreeList<T, Size>::full() const noexcept {
  return data.overhead.count.load() >= max_size();
}

template <typename T, uint64_t Size>
typename FreeList<T, Size>::index_type FreeList<T, Size>::size() const
    noexcept {
  return data.overhead.count.load();
}

template <typename T, uint64_t Size>
typename FreeList<T, Size>::index_type FreeList<T, Size>::max_size() noexcept {
  return kIndexCount - kElementOverheadCount;
}

template <typename T, uint64_t Size>
void FreeList<T, Size>::clear() noexcept {
  // TODO(dj): This could be improved by merge-sorting the free-linked-list

  // Traverse the free list to find already-freed items
  bool itemIsFree[kIndexCount];
  for (index_type i = kElementOverheadCount; i < kIndexCount; ++i) {
    itemIsFree[i] = false;
  }

  index_type curr_free = data.overhead.free.load(std::memory_order_relaxed);
  while (curr_free != 0) {
    itemIsFree[curr_free] = true;
    curr_free = data.elements[curr_free].index;
  }

  // Delete all non-freed items up to next
  const index_type next = data.overhead.next.load(std::memory_order_relaxed);
  for (index_type i = kElementOverheadCount; i < next; ++i) {
    if (!itemIsFree[i]) {
      // Call destructor
      data.elements[i].data.~T();
    }
  }

  data.overhead.free = 0;
  data.overhead.count = 0;
  data.overhead.next = kElementOverheadCount;
}

template <typename T, uint64_t Size>
template <typename... Args>
T* FreeList<T, Size>::alloc(Args... args) {
  index_type index = push_index();
  if (!index) {
    throw std::bad_alloc();
  }

  try {
    // placement-new the item
    return new (get(index)) T(args...);
  } catch (...) {
    pop_index(index);
    throw;
  }
}

template <typename T, uint64_t Size>
template <typename... Args>
typename FreeList<T, Size>::UniquePtr FreeList<T, Size>::make_unique(
    Args... args) {
  T* ptr = alloc(args...);
  return UniquePtr(ptr, deleter());
}

template <typename T, uint64_t Size>
template <typename... Args>
std::shared_ptr<T> FreeList<T, Size>::make_shared(Args... args) {
  return make_unique(args...);
}

template <typename T, uint64_t Size>
typename FreeList<T, Size>::index_type
FreeList<T, Size>::push_index() noexcept {
  // Read the free index
  index_type freeIndex = data.overhead.free.load(std::memory_order_acquire);

  // While free is not zero, then there is a previously-freed item
  while (freeIndex) {
    // Read the index stored at that element
    index_type nextFreeIndex = *reinterpret_cast<index_type*>(get(freeIndex));

    // TODO(dj): Suffers from the ABA problem
    if (data.overhead.free.compare_exchange_strong(freeIndex, nextFreeIndex,
                                                   std::memory_order_release,
                                                   std::memory_order_acquire)) {
      // Return the previously freed item
      ++data.overhead.count;
      return freeIndex;
    }
  }
  // No previously-freed items

  // Atomically get the next index, then increment it
  const index_type next = data.overhead.next++;
  if (next >= kIndexCount) {
    // No next items
    --data.overhead.next;  // Undo the increment
    return 0;
  }

  ++data.overhead.count;
  return next;
}

template <typename T, uint64_t Size>
void FreeList<T, Size>::free(T* item) noexcept {
  assert(item);

  // placement-delete the item
  item->~T();
  pop_index(index(item));
};

template <typename T, uint64_t Size>
void FreeList<T, Size>::pop_index(const index_type index) noexcept {
  assert(index >= kElementOverheadCount);
  assert(index < kIndexCount);

  // We need to atomically:
  // - read the current value of the free index
  // - set the newly-freed-element to contain that free index
  // - change the free index to point to the newly-freed-element
  index_type& freedElement = data.elements[index].index;

  index_type freeIndex = data.overhead.free.load();
  do {
    freedElement = freeIndex;
  } while (data.overhead.free.compare_exchange_strong(
      freeIndex, index));

  --data.overhead.count;
}

template <typename T, uint64_t Size>
T const* FreeList<T, Size>::get(index_type index) const {
  assert(index >= kElementOverheadCount);
  assert(index < kIndexCount);
  return &data.elements[index].data;
}

template <typename T, uint64_t Size>
T* FreeList<T, Size>::get(index_type index) {
  assert(index >= kElementOverheadCount);
  assert(index < kIndexCount);
  return &data.elements[index].data;
}

template <typename T, uint64_t Size>
typename FreeList<T, Size>::index_type FreeList<T, Size>::index(
    T const* item) const {
  assert(item);
  assert(reinterpret_cast<void const*>(item) >=
         reinterpret_cast<void const*>(this));
  assert((reinterpret_cast<uint8_t const*>(item) -
          reinterpret_cast<uint8_t const*>(this)) %
             kElementSize ==
         0);
  index_type index = reinterpret_cast<_element const*>(item) - data.elements;

  assert(index < kIndexCount);
  return index;
}

}  // namespace freelist

#endif  // INCLUDE_FREELIST_FREELIST_H_
