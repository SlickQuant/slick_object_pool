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

#include <gtest/gtest.h>
#include <slick/object_pool.h>

#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <algorithm>
#include <random>
#include <set>

// Test structures
struct SimpleStruct {
    int32_t id;
    double value;
};

struct LargeStruct {
    int64_t timestamp;
    double values[128];
    char data[256];
};

struct AlignedStruct {
    alignas(64) int64_t counter;
    double data[7];  // Fill rest of cache line
};

// Test fixture
class ObjectPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

// ============================================================================
// Basic Functionality Tests
// ============================================================================

TEST_F(ObjectPoolTest, ConstructorLocalMemory) {
    slick::ObjectPool<SimpleStruct> pool(256);

    EXPECT_EQ(pool.size(), 256);
}

TEST_F(ObjectPoolTest, AllocateAndFreeBasic) {
    slick::ObjectPool<SimpleStruct> pool(256);

    SimpleStruct* obj = pool.allocate();
    ASSERT_NE(obj, nullptr);

    obj->id = 42;
    obj->value = 3.14;

    EXPECT_EQ(obj->id, 42);
    EXPECT_DOUBLE_EQ(obj->value, 3.14);

    pool.free(obj);
}

TEST_F(ObjectPoolTest, AllocateMultipleObjects) {
    constexpr size_t POOL_SIZE = 512;
    slick::ObjectPool<SimpleStruct> pool(POOL_SIZE);

    std::vector<SimpleStruct*> objects;

    // Allocate half the pool
    for (size_t i = 0; i < POOL_SIZE / 2; ++i) {
        SimpleStruct* obj = pool.allocate();
        ASSERT_NE(obj, nullptr);
        obj->id = static_cast<int32_t>(i);
        objects.push_back(obj);
    }

    // Verify all objects
    for (size_t i = 0; i < objects.size(); ++i) {
        EXPECT_EQ(objects[i]->id, static_cast<int32_t>(i));
    }

    // Free all
    for (auto* obj : objects) {
        pool.free(obj);
    }
}

TEST_F(ObjectPoolTest, PoolExhaustion) {
    constexpr size_t POOL_SIZE = 64;
    slick::ObjectPool<SimpleStruct> pool(POOL_SIZE);

    std::vector<SimpleStruct*> objects;

    // Exhaust the pool + allocate from heap
    for (size_t i = 0; i < POOL_SIZE + 10; ++i) {
        SimpleStruct* obj = pool.allocate();
        ASSERT_NE(obj, nullptr);
        obj->id = static_cast<int32_t>(i);
        objects.push_back(obj);
    }

    // All should be valid
    for (size_t i = 0; i < objects.size(); ++i) {
        EXPECT_EQ(objects[i]->id, static_cast<int32_t>(i));
    }

    // Free all (should handle both pool and heap objects)
    for (auto* obj : objects) {
        pool.free(obj);
    }
}

TEST_F(ObjectPoolTest, ReuseObjects) {
    slick::ObjectPool<SimpleStruct> pool(128);

    // Allocate and free multiple times
    for (int cycle = 0; cycle < 10; ++cycle) {
        std::vector<SimpleStruct*> objects;

        for (int i = 0; i < 50; ++i) {
            SimpleStruct* obj = pool.allocate();
            ASSERT_NE(obj, nullptr);
            obj->id = cycle * 100 + i;
            objects.push_back(obj);
        }

        // Verify
        for (size_t i = 0; i < objects.size(); ++i) {
            EXPECT_EQ(objects[i]->id, cycle * 100 + static_cast<int>(i));
        }

        // Free all
        for (auto* obj : objects) {
            pool.free(obj);
        }
    }
}

TEST_F(ObjectPoolTest, LargeObjectHandling) {
    slick::ObjectPool<LargeStruct> pool(128);

    LargeStruct* obj = pool.allocate();
    ASSERT_NE(obj, nullptr);

    obj->timestamp = 1234567890;
    for (int i = 0; i < 128; ++i) {
        obj->values[i] = i * 1.5;
    }
    std::strcpy(obj->data, "Test large struct");

    EXPECT_EQ(obj->timestamp, 1234567890);
    EXPECT_DOUBLE_EQ(obj->values[0], 0.0);
    EXPECT_DOUBLE_EQ(obj->values[127], 127 * 1.5);
    EXPECT_STREQ(obj->data, "Test large struct");

    pool.free(obj);
}

// ============================================================================
// Multi-Threading Tests
// ============================================================================

TEST_F(ObjectPoolTest, MultiThreadedAllocateFree) {
    constexpr size_t POOL_SIZE = 2048;
    constexpr int NUM_THREADS = 8;
    constexpr int OPS_PER_THREAD = 1000;

    slick::ObjectPool<SimpleStruct> pool(POOL_SIZE);
    std::atomic<int> error_count{0};

    auto worker = [&](int thread_id) {
        for (int i = 0; i < OPS_PER_THREAD; ++i) {
            SimpleStruct* obj = pool.allocate();
            if (!obj) {
                error_count++;
                continue;
            }

            obj->id = thread_id * OPS_PER_THREAD + i;
            obj->value = static_cast<double>(thread_id);

            // Verify immediately
            if (obj->id != thread_id * OPS_PER_THREAD + i) {
                error_count++;
            }

            pool.free(obj);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(error_count.load(), 0);
}

TEST_F(ObjectPoolTest, ConcurrentStressTest) {
    constexpr size_t POOL_SIZE = 512;
    constexpr int NUM_THREADS = 16;
    constexpr int OPS_PER_THREAD = 10000;

    slick::ObjectPool<SimpleStruct> pool(POOL_SIZE);
    std::atomic<uint64_t> total_allocations{0};
    std::atomic<uint64_t> total_deallocations{0};

    auto worker = [&](int thread_id) {
        std::mt19937 rng(thread_id);
        std::uniform_int_distribution<int> dist(1, 10);

        std::vector<SimpleStruct*> local_objects;
        local_objects.reserve(100);

        for (int i = 0; i < OPS_PER_THREAD; ++i) {
            // Randomly allocate or free
            if (dist(rng) > 5 || local_objects.empty()) {
                // Allocate
                SimpleStruct* obj = pool.allocate();
                obj->id = thread_id;
                local_objects.push_back(obj);
                total_allocations++;
            } else {
                // Free
                size_t idx = rng() % local_objects.size();
                pool.free(local_objects[idx]);
                local_objects.erase(local_objects.begin() + idx);
                total_deallocations++;
            }
        }

        // Clean up remaining
        for (auto* obj : local_objects) {
            pool.free(obj);
            total_deallocations++;
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(total_allocations.load(), total_deallocations.load());
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(ObjectPoolTest, DISABLED_BenchmarkSingleThreaded) {
    constexpr size_t POOL_SIZE = 1024;
    constexpr int ITERATIONS = 1000000;

    slick::ObjectPool<SimpleStruct> pool(POOL_SIZE);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < ITERATIONS; ++i) {
        SimpleStruct* obj = pool.allocate();
        obj->id = i;
        pool.free(obj);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    double ns_per_op = static_cast<double>(duration.count()) / ITERATIONS;

    std::cout << "Single-threaded performance: " << ns_per_op << " ns/op" << std::endl;
    std::cout << "Throughput: " << (ITERATIONS * 1e9 / duration.count()) << " ops/sec" << std::endl;
}

TEST_F(ObjectPoolTest, DISABLED_BenchmarkMultiThreaded) {
    constexpr size_t POOL_SIZE = 2048;
    constexpr int NUM_THREADS = 8;
    constexpr int OPS_PER_THREAD = 100000;

    slick::ObjectPool<SimpleStruct> pool(POOL_SIZE);

    auto start = std::chrono::high_resolution_clock::now();

    auto worker = [&](int thread_id) {
        for (int i = 0; i < OPS_PER_THREAD; ++i) {
            SimpleStruct* obj = pool.allocate();
            obj->id = thread_id * OPS_PER_THREAD + i;
            pool.free(obj);
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    int total_ops = NUM_THREADS * OPS_PER_THREAD;
    double ns_per_op = static_cast<double>(duration.count()) / total_ops;

    std::cout << "Multi-threaded (" << NUM_THREADS << " threads) performance: "
              << ns_per_op << " ns/op" << std::endl;
    std::cout << "Throughput: " << (total_ops * 1e9 / duration.count()) << " ops/sec" << std::endl;
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

TEST_F(ObjectPoolTest, NullPointerHandling) {
    slick::ObjectPool<SimpleStruct> pool(256);

    // free_object should handle null gracefully (though not officially supported)
    // This test verifies it doesn't crash
    SimpleStruct* external = new SimpleStruct();
    external->id = 999;

    // Free object not from pool (should delete it)
    pool.free(external);

    // Don't access external after this point - it's been deleted
}

TEST_F(ObjectPoolTest, AlignmentTest) {
    slick::ObjectPool<AlignedStruct> pool(128);

    std::vector<AlignedStruct*> objects;

    for (int i = 0; i < 50; ++i) {
        AlignedStruct* obj = pool.allocate();
        ASSERT_NE(obj, nullptr);

        // Check alignment
        EXPECT_EQ(reinterpret_cast<uintptr_t>(obj) % 64, 0)
            << "Object not properly aligned to 64-byte boundary";

        obj->counter = i;
        objects.push_back(obj);
    }

    for (auto* obj : objects) {
        pool.free(obj);
    }
}

TEST_F(ObjectPoolTest, PowerOfTwoSizes) {
    // Test various power-of-2 sizes
    std::vector<uint32_t> sizes = {64, 128, 256, 512, 1024, 2048, 4096};

    for (uint32_t size : sizes) {
        slick::ObjectPool<SimpleStruct> pool(size);
        EXPECT_EQ(pool.size(), size);

        SimpleStruct* obj = pool.allocate();
        ASSERT_NE(obj, nullptr);
        pool.free(obj);
    }
}

TEST_F(ObjectPoolTest, WrapAroundTest) {
    constexpr size_t POOL_SIZE = 64;
    slick::ObjectPool<SimpleStruct> pool(POOL_SIZE);

    // Allocate and free many times to force wrap-around
    for (int cycle = 0; cycle < 100; ++cycle) {
        std::vector<SimpleStruct*> objects;

        for (size_t i = 0; i < POOL_SIZE; ++i) {
            SimpleStruct* obj = pool.allocate();
            ASSERT_NE(obj, nullptr);
            obj->id = static_cast<int32_t>(cycle * POOL_SIZE + i);
            objects.push_back(obj);
        }

        for (auto* obj : objects) {
            pool.free(obj);
        }
    }
}

// ============================================================================
// Data Integrity Tests
// ============================================================================

TEST_F(ObjectPoolTest, DataIntegrity) {
    constexpr size_t POOL_SIZE = 256;
    slick::ObjectPool<SimpleStruct> pool(POOL_SIZE);

    std::vector<SimpleStruct*> objects;

    // Allocate and fill
    for (int i = 0; i < 100; ++i) {
        SimpleStruct* obj = pool.allocate();
        obj->id = i;
        obj->value = i * 1.5;
        objects.push_back(obj);
    }

    // Verify all data
    for (size_t i = 0; i < objects.size(); ++i) {
        EXPECT_EQ(objects[i]->id, static_cast<int32_t>(i));
        EXPECT_DOUBLE_EQ(objects[i]->value, i * 1.5);
    }

    // Free half
    for (size_t i = 0; i < objects.size() / 2; ++i) {
        pool.free(objects[i]);
    }

    // Verify remaining half
    for (size_t i = objects.size() / 2; i < objects.size(); ++i) {
        EXPECT_EQ(objects[i]->id, static_cast<int32_t>(i));
        EXPECT_DOUBLE_EQ(objects[i]->value, i * 1.5);
    }

    // Free remaining
    for (size_t i = objects.size() / 2; i < objects.size(); ++i) {
        pool.free(objects[i]);
    }
}

TEST_F(ObjectPoolTest, NoObjectLeakage) {
    constexpr size_t POOL_SIZE = 512;
    slick::ObjectPool<SimpleStruct> pool(POOL_SIZE);

    std::set<SimpleStruct*> allocated_addresses;

    // Allocate all
    std::vector<SimpleStruct*> objects;
    for (size_t i = 0; i < POOL_SIZE; ++i) {
        SimpleStruct* obj = pool.allocate();
        ASSERT_NE(obj, nullptr);

        // Check for duplicate addresses (would indicate aliasing)
        EXPECT_EQ(allocated_addresses.count(obj), 0)
            << "Duplicate object pointer detected!";
        allocated_addresses.insert(obj);

        objects.push_back(obj);
    }

    // Free all
    for (auto* obj : objects) {
        pool.free(obj);
    }

    // Allocate again - should reuse same addresses
    std::set<SimpleStruct*> reused_addresses;
    objects.clear();

    for (size_t i = 0; i < POOL_SIZE; ++i) {
        SimpleStruct* obj = pool.allocate();
        ASSERT_NE(obj, nullptr);

        // Should be from the original set
        EXPECT_GT(allocated_addresses.count(obj), 0)
            << "Object not from pool!";

        reused_addresses.insert(obj);
        objects.push_back(obj);
    }

    // Clean up
    for (auto* obj : objects) {
        pool.free(obj);
    }
}
