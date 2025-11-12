/********************************************************************************
 * Copyright (c) 2025 SlickQuant
 * All rights reserved
 *
 * This file is part of the slick_object_pool. Redistribution and use in source and
 * binary forms, with or without modification, are permitted exclusively under
 * the terms of the MIT license which is available at
 * https://github.com/SlickQuant/slick_object_pool/blob/main/LICENSE
 *
 ********************************************************************************/

#pragma once

#include <cstdint>
#include <atomic>
#include <stdexcept>
#include <string>
#include <cassert>
#include <limits>

namespace slick {

/**
 * @file object_pool.h
 * @brief Lock-free, cache-optimized object pool for high-performance allocation
 *
 * @details
 * This object pool implementation provides:
 * - Lock-free multi-producer multi-consumer (MPMC) architecture
 * - Cache-line alignment to eliminate false sharing
 * - O(1) allocation and deallocation
 * - Zero external dependencies (standard library only)
 * - Automatic heap allocation fallback when pool is exhausted
 *
 * @section memory_layout Memory Layout
 *
 * @code
 * [Cache Line 0: reserved_     Producer atomics (separate cache line)]
 * [Cache Line 1: consumed_     Consumer atomics (separate cache line)]
 * [Heap:         control_      Ring buffer metadata]
 * [Heap:         buffer_       Pooled objects]
 * [Heap:         free_objects_ Free object pointers]
 * @endcode
 *
 * @section thread_safety Thread Safety
 * - Multiple threads can call allocate() concurrently (lock-free)
 * - Multiple threads can call free() concurrently (lock-free)
 * - reset() is NOT thread-safe
 *
 * @section example Example Usage
 * @code
 * slick::ObjectPool<MyStruct> pool(1024);
 * auto* obj = pool.allocate();
 * obj->field = value;
 * pool.free(obj);
 * @endcode
 *
 * @tparam T Object type to pool
 *
 * @author SlickQuant
 * @version 0.0.1
 * @date 2025
 * @copyright MIT License
 */
template<typename T>
class ObjectPool {
    // Type safety check: T must be default constructible
    static_assert(std::is_default_constructible_v<T>,
        "T must be default constructible");

    /// Hardware cache line size (typically 64 bytes, auto-detected if available)
#ifdef __cpp_lib_hardware_interference_size
    static constexpr size_t CACHE_LINE_SIZE = std::hardware_destructive_interference_size;
#else
    static constexpr size_t CACHE_LINE_SIZE = 64;
#endif

    /**
     * @brief Ring buffer slot metadata
     * @details Tracks the data index and size for each slot in the ring buffer
     */
    struct slot {
        std::atomic_uint_fast64_t data_index{ std::numeric_limits<uint64_t>::max() };  ///< Absolute index of data in this slot
        uint32_t size = 1;  ///< Number of consecutive slots occupied
    };

    /**
     * @brief Producer reservation information
     * @details Tracks the current write position and reservation size
     */
    struct reserved_info {
        uint_fast64_t index_ = 0;  ///< Next available write index
        uint_fast32_t size_ = 0;   ///< Size of current reservation
    };

    // Cache-line aligned atomics to prevent false sharing
    alignas(CACHE_LINE_SIZE) std::atomic<reserved_info> reserved_;  ///< Producer reservation counter (own cache line)
    alignas(CACHE_LINE_SIZE) std::atomic_uint_fast64_t consumed_;   ///< Consumer consumption counter (own cache line)

    // Object storage
    uint32_t size_;                 ///< Pool capacity (must be power of 2)
    uint32_t mask_;                 ///< Bitmask for index wrapping (size_ - 1)
    T* buffer_ = nullptr;           ///< Array of pooled objects
    intptr_t lower_bound_ = 0;      ///< Lower address bound for pool ownership check
    intptr_t upper_bound_ = 0;      ///< Upper address bound for pool ownership check
    T** free_objects_ = nullptr;    ///< Array of pointers to free objects
    slot* control_ = nullptr;       ///< Ring buffer control slots

public:
    /**
     * @brief Construct a new object pool
     *
     * @details
     * Creates a new object pool with local memory allocation.
     * The pool size must be a power of 2 for efficient bit-masking operations.
     * All objects are pre-allocated and initialized.
     *
     * @param size Pool capacity (must be power of 2)
     *
     * @throws std::runtime_error If size is not a power of 2
     *
     * @post Pool is ready for concurrent allocate/free operations
     *
     * @par Example
     * @code
     * slick::ObjectPool<MyStruct> pool(256);
     * @endcode
     */
    ObjectPool(uint32_t size)
        : size_(size)
        , mask_(size - 1)
        , buffer_(new T[size_])
        , free_objects_(new T*[size_])
        , control_(new slot[size_])
    {
        assert((size && !(size & (size - 1))) && "size must be power of 2");

        // Initialize pool with all objects available
        uint64_t index;
        for (uint32_t i = 0; i < size_; ++i) {
            index = reserve();
            free_objects_[index & mask_] = &buffer_[i];
            publish(index);
        }

        lower_bound_ = reinterpret_cast<intptr_t>(&buffer_[0]);
        upper_bound_ = reinterpret_cast<intptr_t>(&buffer_[mask_]);
    }

    /**
     * @brief Destructor - cleans up all resources
     */
    virtual ~ObjectPool() noexcept {
        delete[] buffer_;
        buffer_ = nullptr;

        delete[] free_objects_;
        free_objects_ = nullptr;

        delete[] control_;
        control_ = nullptr;
    }

    // Delete copy and move operations
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;
    ObjectPool(ObjectPool&&) = delete;
    ObjectPool& operator=(ObjectPool&&) = delete;

    /**
     * @brief Get pool capacity
     * @return Maximum number of objects the pool can hold
     */
    uint32_t size() const noexcept {
        return size_;
    }

    /**
     * @brief Allocate an object from the pool
     *
     * @details
     * Returns a pre-allocated object from the pool if available.
     * If the pool is exhausted, allocates from heap as fallback.
     * This method is lock-free and thread-safe.
     *
     * @return Pointer to allocated object (never nullptr)
     *
     * @note Objects allocated from heap (when pool exhausted) will be
     *       automatically deleted when returned via free_object()
     *
     * @par Thread Safety
     * Safe to call concurrently from multiple threads
     *
     * @par Example
     * @code
     * auto* obj = pool.allocate();
     * obj->value = 42;
     * @endcode
     */
    T* allocate() {
        auto [obj, size] = consume();
        if (!obj) {
            // Pool exhausted - allocate from heap
            return new T();
        }
        assert(size == 1);
        return obj;
    }

    /**
     * @brief Return an object to the pool
     *
     * @details
     * Returns an object to the pool for reuse, or deletes it if it didn't
     * come from the pool. Uses lock-free operations for thread safety.
     *
     * This method automatically detects whether the object belongs to the
     * pool by checking its address range. Objects allocated from heap
     * (when pool was exhausted) are automatically deleted.
     *
     * @param obj Pointer to object to return (must not be nullptr)
     *
     * @par Thread Safety
     * Safe to call concurrently from multiple threads
     *
     * @par Example
     * @code
     * pool.free_object(obj);  // Returns to pool or deletes
     * @endcode
     *
     * @warning Do not call with nullptr
     * @warning Do not free the same object twice
     * @warning Do not access object after calling free_object()
     */
    void free(T* obj) {
        auto o = reinterpret_cast<intptr_t>(obj);
        if (o >= lower_bound_ && o <= upper_bound_) {
            // Object belongs to pool - return it
            auto index = reserve();
            free_objects_[index & mask_] = obj;
            publish(index);
        } else {
            // Object was heap-allocated - delete it
            delete obj;
        }
    }

    /**
     * @brief Reset the pool to initial state
     *
     * @details
     * Reinitializes the pool, making all objects available again.
     * This is primarily intended for testing and should be used with caution.
     *
     * @warning NOT THREAD-SAFE
     * @warning Must be called when NO other threads are accessing the pool
     * @warning Invalidates all outstanding object references
     *
     * @par Use Cases
     * - Unit testing: Reset pool state between tests
     * - Application reset: Clear all allocations during shutdown
     *
     * @note Do not use in production with concurrent access
     */
    void reset() noexcept {
        delete[] control_;
        control_ = new slot[size_];

        reserved_.store(reserved_info(), std::memory_order_release);
        uint64_t index;
        for (uint32_t i = 0; i < size_; ++i) {
            index = reserve();
            free_objects_[index & mask_] = &buffer_[i];
            publish(index);
        }
        consumed_.store(0, std::memory_order_release);
    }

private:
    /**
     * @brief Get the initial reading index
     * @return Starting index for consumption
     */
    uint64_t get_read_index() const noexcept {
        return consumed_.load(std::memory_order_acquire);
    }

    /**
     * @brief Reserve space in the ring buffer for writing
     *
     * @details
     * Atomically reserves n slots in the ring buffer using compare-and-swap.
     * Handles ring buffer wrapping when reaching the end.
     *
     * @param n Number of slots to reserve (default: 1)
     * @return Starting index of the reserved space
     *
     * @throws std::runtime_error If n exceeds pool size
     *
     * @note Lock-free operation using CAS
     * @note May retry multiple times under high contention
     */
    uint64_t reserve(uint32_t n = 1) {
        if (n > size_) [[unlikely]] {
            throw std::runtime_error("required size " + std::to_string(n) + " > pool size " + std::to_string(size_));
        }
        auto reserved = reserved_.load(std::memory_order_relaxed);
        reserved_info next;
        uint64_t index;
        bool buffer_wrapped = false;
        do {
            buffer_wrapped = false;
            next = reserved;
            index = reserved.index_;
            auto idx = index & mask_;
            if ((idx + n) > size_) {
                // Not enough buffer left, wrap to beginning
                index += size_ - idx;
                next.index_ = index + n;
                next.size_ = n;
                buffer_wrapped = true;
            }
            else {
                next.index_ += n;
                next.size_ = n;
            }
        } while(!reserved_.compare_exchange_weak(reserved, next, std::memory_order_release, std::memory_order_relaxed));

        if (buffer_wrapped) {
            // queue wrapped, set current slock.data_index to the reserved index to let the reader
            // know the next available data is in different slot.
            auto& slot = control_[reserved.index_ & mask_];
            slot.size = n;
            slot.data_index.store(index, std::memory_order_release);
        }
        return index;
    }

    /**
     * @brief Access reserved space for writing
     * @param index Index returned by reserve()
     * @return Pointer to array position
     */
    T** operator[] (uint64_t index) noexcept {
        return &free_objects_[index & mask_];
    }

    /**
     * @brief Access reserved space for writing (const version)
     * @param index Index returned by reserve()
     * @return Const pointer to array position
     */
    const T** operator[] (uint64_t index) const noexcept {
        return &free_objects_[index & mask_];
    }

    /**
     * @brief Publish data written to reserved space
     *
     * @details
     * Makes previously reserved and written data visible to consumers.
     * Must be called after writing to reserved space.
     *
     * @param index Index returned by reserve()
     * @param n Number of slots to publish (default: 1)
     *
     * @note Uses release memory ordering for synchronization
     */
    void publish(uint64_t index, uint32_t n = 1) noexcept {
        auto& slot = control_[index & mask_];
        slot.size = n;
        slot.data_index.store(index, std::memory_order_release);
    }

    /**
     * @brief Consume data from the ring buffer
     *
     * @details
     * Atomically retrieves the next available object from the pool.
     * Returns {nullptr, 0} if no objects are currently available.
     *
     * @return Pair of {object_pointer, slot_count}
     *         Returns {nullptr, 0} if pool is empty
     *
     * @note Lock-free operation using CAS
     * @note May retry multiple times under high contention
     */
    std::pair<T*, uint32_t> consume() noexcept {
        while (true) {
            uint64_t current_index = consumed_.load(std::memory_order_acquire);
            auto current = current_index & mask_;
            slot* current_slot = &control_[current];
            uint64_t stored_index = current_slot->data_index.load(std::memory_order_acquire);

            if (stored_index != std::numeric_limits<uint64_t>::max() && reserved_.load(std::memory_order_relaxed).index_ < stored_index) [[unlikely]] {
                // queue has been reset
                consumed_.store(0, std::memory_order_release);
                continue;
            }

            if (stored_index == std::numeric_limits<uint64_t>::max() || stored_index < current_index) {
                // no more data available
                return std::make_pair(nullptr, 0);
            }
            else if (stored_index > current_index && ((stored_index & mask_) != current)) [[unlikely]] {
                // queue wrapped, skip the unused slots
                consumed_.compare_exchange_weak(current_index, stored_index, std::memory_order_release, std::memory_order_relaxed);
                continue;
            }

            // Try to atomically claim this item
            uint64_t next_index = stored_index + current_slot->size;
            if (consumed_.compare_exchange_weak(current_index, next_index, std::memory_order_release, std::memory_order_relaxed)) {
                // Successfully claimed the item
                return std::make_pair(free_objects_[current_index & mask_], current_slot->size);
            }
            // CAS failed, another consumer claimed it, retry
        }
    }
};

}   // end namespace slick
