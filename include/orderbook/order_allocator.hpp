#pragma once

#include "memory_pool.hpp"
#include "order.hpp"

namespace orderbook {

class OrderAllocator {
public:
  static Order *create(uint64_t id, Side side, double price,
                       uint32_t quantity) {
    return pool.allocate(id, side, price, quantity);
  }

  static void destroy(Order *order) { pool.deallocate(order); }

  static size_t get_allocated_block_count() {
    return pool.get_allocated_block_count();
  }

  static size_t get_active_order_count() {
    return pool.get_active_object_count();
  }

private:
  static inline MemoryPool<Order> pool;
};

} // namespace orderbook
