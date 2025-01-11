#pragma once

#include <array>
#include <atomic>
#include <mutex>
#include <type_traits>

namespace orderbook {

// Constants for tuning
constexpr size_t BLOCK_SIZE = 4096; // 4KB blocks
constexpr size_t MAX_BLOCKS = 1024; // Max number of blocks to allocate

template <typename T, size_t BlockSize = BLOCK_SIZE> class MemoryPool {
  static_assert(BlockSize >= sizeof(T), "BlockSize too small for type T");
  static_assert(BlockSize >= sizeof(void *),
                "BlockSize too small for free list");
  static_assert(std::is_trivially_copyable_v<T>,
                "T must be trivially copyable");

  // Block structure for memory management
  struct alignas(std::max_align_t) Block {
    std::array<std::byte, BlockSize> data;
    Block *next;
    std::atomic<size_t> used_count{0};
  };

  // Free list node structure
  struct FreeNode {
    FreeNode *next;
  };

public:
  MemoryPool()
      : head_block(nullptr), free_list(nullptr), total_active_count(0) {
    allocate_block();
  }

  // Allocate and construct object
  template <typename... Args> T *allocate(Args &&...args) {
    void *ptr = allocate_raw();
    total_active_count.fetch_add(1, std::memory_order_release);
    return new (ptr) T(std::forward<Args>(args)...);
  }

  // Deallocate object
  void deallocate(T *ptr) {
    if (!ptr)
      return;

    // Call destructor
    ptr->~T();

    {
      std::lock_guard<std::mutex> lock(mutex);
      // Add to free list
      auto *node = reinterpret_cast<FreeNode *>(ptr);
      node->next = free_list;
      free_list = node;
    }

    total_active_count.fetch_sub(1, std::memory_order_release);
  }

  ~MemoryPool() {
    Block *current = head_block;
    while (current != nullptr) {
      Block *next = current->next;
      delete current;
      current = next;
    }
  }

  // Statistics
  size_t get_allocated_block_count() const {
    size_t count = 0;
    Block *current = head_block;
    while (current != nullptr) {
      count++;
      current = current->next;
    }
    return count;
  }

  size_t get_active_object_count() const {
    return total_active_count.load(std::memory_order_acquire);
  }

private:
  void allocate_block() {
    if (get_allocated_block_count() >= MAX_BLOCKS) {
      throw std::runtime_error("Maximum block count exceeded");
    }

    auto *new_block = new Block();
    new_block->next = head_block;
    head_block = new_block;
    current_offset = 0;
  }

  void *allocate_raw() {
    std::lock_guard<std::mutex> lock(mutex);

    // Try to reuse from free list first
    if (free_list != nullptr) {
      void *ptr = free_list;
      free_list = free_list->next;
      return ptr;
    }

    // If current block is full, allocate new block
    if (current_offset + sizeof(T) > BlockSize) {
      allocate_block();
    }

    // Allocate from current block
    void *ptr = &head_block->data[current_offset];
    current_offset += sizeof(T);
    head_block->used_count.fetch_add(1, std::memory_order_release);

    return ptr;
  }

  Block *head_block;
  FreeNode *free_list;
  size_t current_offset{0};
  std::mutex mutex;
  std::atomic<size_t> total_active_count{0};
};

} // namespace orderbook
