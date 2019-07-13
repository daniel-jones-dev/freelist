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
#include <utility>

namespace freelist {

template <typename T, uint64_t Size>
class FreeListAllocator;

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

  /**
   * Type that is stored in each active element.
   */
  using value_type = T;

  using this_type = FreeList<T, Size>;

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

  using size_type = uint64_t;

  /**
   * Type of function used to free items.
   */
  using Deleter = std::function<void(T*)>;

  /**
   * Type of std::unique_ptr returned by make_unique.
   */
  using UniquePtr = std::unique_ptr<T, Deleter>;

  /**
   * Type of an allocator created from this FreeList.
   */
  using Allocator = FreeListAllocator<T, Size>;

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
  inline index_type index(T* item);

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

  inline Allocator allocator() { return Allocator(*this); }

 private:
  union Element {
    // Define empty constructor and destructor - defaults are ill-formed
    Element() {}
    ~Element() {}

    T data;
    index_type index;
  };

  struct ControlFlags {
    ControlFlags() = default;
    ControlFlags(ControlFlags const& other) = default;
    ~ControlFlags() = default;

    index_type next{0};
    index_type count{0};
    index_type free{0};
    index_type tag{0};
  };

  static constexpr uint64_t kElementSize = sizeof(Element);

  static constexpr index_type kIndexCount = Size / kElementSize;

  union _data {
    // Define empty constructor and destructor - defaults are ill-formed
    _data() {}
    ~_data() {}

    std::atomic<ControlFlags> control;
    Element elements[kIndexCount];
  } data;

  static constexpr index_type kElementOverheadCount =
      (sizeof(data.control) + kElementSize - 1) / kElementSize;

  static_assert(alignof(data.control) < sizeof(T) ||
                    Size % alignof(data.control) == 0 ||
                    sizeof(index_type) != 1,
                "Size must be a multiple of 4 (for Size <= 256)");
  static_assert(alignof(data.control) < sizeof(T) ||
                    Size % alignof(data.control) == 0 ||
                    sizeof(index_type) != 2,
                "Size must be a multiple of 8 (for Size <= 131072)");
  static_assert(alignof(data.control) < sizeof(T) ||
                    Size % alignof(data.control) == 0 ||
                    sizeof(index_type) != 4,
                "Size must be a multiple of 16 (for Size <= 17179869184)");
  static_assert(alignof(data.control) < sizeof(T) ||
                    Size % alignof(data.control) == 0 ||
                    sizeof(index_type) != 8,
                "Size must be a multiple of 32 (for Size > 17179869184)");

  static_assert(alignof(data.control) >= sizeof(T) || Size % alignof(T) == 0,
                "Size must be a multiple of alignof(T)");

  static_assert(kIndexCount > kElementOverheadCount,
                "FreeList is too small to contain an element");
};

template <typename T, uint64_t Size>
FreeList<T, Size>::FreeList() {
  ControlFlags flags;
  flags.next = kElementOverheadCount;
  flags.count = 0;
  flags.free = 0;
  flags.tag = 0;
  data.control.store(flags);
}

template <typename T, uint64_t Size>
FreeList<T, Size>::~FreeList() noexcept {
  clear();
}

template <typename T, uint64_t Size>
bool FreeList<T, Size>::empty() const noexcept {
  return data.control.load().count == 0;
}

template <typename T, uint64_t Size>
bool FreeList<T, Size>::full() const noexcept {
  return data.control.load().count >= max_size();
}

template <typename T, uint64_t Size>
typename FreeList<T, Size>::index_type FreeList<T, Size>::size() const
    noexcept {
  return data.control.load().count;
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

  ControlFlags currFlags = data.control.load(std::memory_order_relaxed);
  while (currFlags.free != 0) {
    itemIsFree[currFlags.free] = true;
    currFlags.free = data.elements[currFlags.free].index;
  }

  // Delete all non-freed items up to next
  for (index_type i = kElementOverheadCount; i < currFlags.next; ++i) {
    if (!itemIsFree[i]) {
      // Call destructor
      data.elements[i].data.~T();
    }
  }

  currFlags.free = 0;
  currFlags.tag = 0;
  currFlags.count = 0;
  currFlags.next = kElementOverheadCount;
  data.control.store(currFlags);
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
  ControlFlags currFlags = data.control.load();

  do {
    ControlFlags newFlags = currFlags;
    if (currFlags.free) {
      // While free is not zero, then there is a previously-freed item

      // Read the index stored at that element
      newFlags.free = *reinterpret_cast<index_type*>(get(currFlags.free));
      // Increment the tag to avoid the ABA problem
      ++newFlags.tag;
      // Creating a new item
      ++newFlags.count;

      if (data.control.compare_exchange_strong(currFlags, newFlags)) {
        // Return the previously freed item
        return currFlags.free;
      }
    } else if (currFlags.next < kIndexCount) {
      // Increment the next
      ++newFlags.next;
      // Creating a new item
      ++newFlags.count;

      if (data.control.compare_exchange_strong(currFlags, newFlags)) {
        // Return the pre-increment next index
        return currFlags.next;
      }
    } else {
      return 0;
    }
  } while (1);
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
  // - include a tag increment to avoid ABA problem
  index_type& freedElement = data.elements[index].index;

  ControlFlags currFlags = data.control.load();
  ControlFlags newFlags;
  do {
    newFlags = currFlags;
    freedElement = currFlags.free;
    newFlags.free = index;
    ++newFlags.tag;
    --newFlags.count;
  } while (!data.control.compare_exchange_strong(currFlags, newFlags));
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
typename FreeList<T, Size>::index_type FreeList<T, Size>::index(T* item) {
  assert(item);
  assert(reinterpret_cast<void*>(item) >= reinterpret_cast<void*>(this));
  assert((reinterpret_cast<uint8_t*>(item) - reinterpret_cast<uint8_t*>(this)) %
             kElementSize ==
         0);
  index_type index = reinterpret_cast<Element*>(item) - data.elements;

  assert(index < kIndexCount);
  return index;
}

template <typename T, uint64_t Size>
typename FreeList<T, Size>::index_type FreeList<T, Size>::index(
    T const* item) const {
  return const_cast<this_type*>(this)->index(const_cast<T*>(item));
}

template <typename T, uint64_t Size>
class FreeListAllocator {
 public:
  using FreeListType = FreeList<T, Size>;
  using value_type = T;
  using pointer = T*;
  using const_pointer = const T*;
  using void_pointer = void*;
  using size_type = typename FreeListType::size_type;
  using difference_type = std::ptrdiff_t;

  template <typename U>
  struct rebind {
    using other = FreeListAllocator<U, Size>;
  };

  explicit FreeListAllocator(FreeListType& parent) : parent(parent) {}

  FreeListAllocator(FreeListAllocator const&) = default;
  ~FreeListAllocator() = default;

  T* allocate(std::size_t n) {
    if (n != 1) {
      throw std::bad_alloc();
    }
    typename FreeListType::index_type index = parent.push_index();
    if (index) {
      return parent.get(index);
    } else {
      throw std::bad_alloc();
    }
  }

  void deallocate(T* p, std::size_t) noexcept {
    parent.pop_index(parent.index(p));
  }

 private:
  FreeListType& parent;
};

}  // namespace freelist

#endif  // INCLUDE_FREELIST_FREELIST_H_
