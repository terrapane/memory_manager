/*
 *  test_memory_allocator.cpp
 *
 *  Copyright (C) 2024
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Description:
 *      This module will test the MemoryAllocator object.
 *
 *  Portability Issues:
 *      None.
 */

#include <vector>
#include <cstring>
#include <terra/memory_manager/memory_manager.h>
#include <terra/memory_manager/memory_allocator.h>
#include <terra/stf/stf.h>

// Used to ensure symmetric calls are made
static unsigned Allocations = 0;
static unsigned Deallocations = 0;

// Define a derived class to catch calls to allocate/deallocate
template<typename T>
struct TestAllocator : public Terra::MemoryManager::MemoryAllocator<T>
{
    // Constructors and destructor
    constexpr TestAllocator(
        Terra::MemoryManager::MemoryManagerPointer &memory_manager) :
        Terra::MemoryManager::MemoryAllocator<T>(memory_manager)
    {
    }
    constexpr ~TestAllocator() = default;

    constexpr T *allocate(std::size_t n)
    {
        Allocations++;
        return Terra::MemoryManager::MemoryAllocator<T>::allocate(n);
    }
    constexpr void deallocate(T *p, std::size_t n) noexcept
    {
        Deallocations++;
        Terra::MemoryManager::MemoryAllocator<T>::deallocate(p, n);
    }
};

STF_TEST(MemoryAllocator, TestVector1)
{
    // Define the memory profile
    Terra::MemoryManager::MemoryProfile profile =
    {
        // Size, Minimum, Maximum, Excess Allowed
        {    64,       5,      10, true  },
        {   256,       2,      10, true  },
        {   512,       2,      10, true  },
        {  1024,       1,      20, true  },
        { 65536,       0,       1, true  }
    };

    // Create a Memory Manager for the given profile
    Terra::MemoryManager::MemoryManagerPointer memory_manager =
        std::make_shared<Terra::MemoryManager::MemoryManager>(profile);

    {
        // Get the statistics
        auto stats = memory_manager->GetStatistics();

        // There should be 5 sets of statistics, one for each descriptor
        STF_ASSERT_EQ(5, stats.size());

        // Iterate over each statistic and check expected values
        for (std::size_t i = 0; i < stats.size(); i++)
        {
            STF_ASSERT_EQ(profile[i].size, stats[i].size);
            STF_ASSERT_EQ(0, stats[i].allocations);
            STF_ASSERT_EQ(0, stats[i].deallocations);
            STF_ASSERT_EQ(0, stats[i].corruption_count);
            STF_ASSERT_EQ(0, stats[i].outstanding);
            STF_ASSERT_EQ(0, stats[i].max_outstanding);
            STF_ASSERT_EQ(0, stats[i].unfulfilled);
        }
    }

    // Reset globals
    Allocations = 0;
    Deallocations = 0;

    {
        std::vector<int, TestAllocator<int>> vector(memory_manager);

        vector.push_back(1);
        vector.push_back(2);
        vector.push_back(3);
        vector.push_back(4);
        vector.resize(100);
    }

    // There should be an equal number of allocations and deallocations
    STF_ASSERT_EQ(Allocations, Deallocations);

    // Ensure there was at least 1 allocation
    STF_ASSERT_GT(Allocations, 0);

    {
        // Get the statistics
        auto stats = memory_manager->GetStatistics();

        // There should be 5 sets of statistics, one for each descriptor
        STF_ASSERT_EQ(5, stats.size());

        // Was memory allocated in the Memory Manager?
        bool allocated = false;

        // Iterate over each statistic and check expected values
        for (std::size_t i = 0; i < stats.size(); i++)
        {
            STF_ASSERT_EQ(profile[i].size, stats[i].size);
            STF_ASSERT_EQ(stats[i].allocations, stats[i].deallocations);
            STF_ASSERT_EQ(0, stats[i].corruption_count);
            STF_ASSERT_EQ(0, stats[i].outstanding);
            STF_ASSERT_EQ(0, stats[i].unfulfilled);
            if (stats[i].allocations > 0) allocated = true;
        }

        // Ensure memory was allocated via the Memory Manager
        STF_ASSERT_TRUE(allocated);
    }
}

STF_TEST(MemoryAllocator, TestVector2)
{
    // Define the memory profile
    Terra::MemoryManager::MemoryProfile profile =
    {
        // Size, Minimum, Maximum, Excess Allowed
        {    64,       5,      10, true  },
        {   256,       2,      10, true  },
        {   512,       2,      10, true  },
        {  1024,       1,      20, true  },
        { 65536,       0,       1, true  }
    };

    // Create a Memory Manager for the given profile
    Terra::MemoryManager::MemoryManagerPointer memory_manager =
        std::make_shared<Terra::MemoryManager::MemoryManager>(profile);

    {
        // Get the statistics
        auto stats = memory_manager->GetStatistics();

        // There should be 5 sets of statistics, one for each descriptor
        STF_ASSERT_EQ(5, stats.size());

        // Iterate over each statistic and check expected values
        for (std::size_t i = 0; i < stats.size(); i++)
        {
            STF_ASSERT_EQ(profile[i].size, stats[i].size);
            STF_ASSERT_EQ(0, stats[i].allocations);
            STF_ASSERT_EQ(0, stats[i].deallocations);
            STF_ASSERT_EQ(0, stats[i].corruption_count);
            STF_ASSERT_EQ(0, stats[i].outstanding);
            STF_ASSERT_EQ(0, stats[i].max_outstanding);
            STF_ASSERT_EQ(0, stats[i].unfulfilled);
        }
    }

    {
        std::vector<int, Terra::MemoryManager::MemoryAllocator<int>> vector(
            memory_manager);

        vector.push_back(1);
        vector.push_back(2);
        vector.push_back(3);
        vector.push_back(4);
        vector.resize(100);
    }

    {
        // Get the statistics
        auto stats = memory_manager->GetStatistics();

        // There should be 5 sets of statistics, one for each descriptor
        STF_ASSERT_EQ(5, stats.size());

        // Was memory allocated in the Memory Manager?
        bool allocated = false;

        // Iterate over each statistic and check expected values
        for (std::size_t i = 0; i < stats.size(); i++)
        {
            STF_ASSERT_EQ(profile[i].size, stats[i].size);
            STF_ASSERT_EQ(stats[i].allocations, stats[i].deallocations);
            STF_ASSERT_EQ(0, stats[i].corruption_count);
            STF_ASSERT_EQ(0, stats[i].outstanding);
            STF_ASSERT_EQ(0, stats[i].unfulfilled);
            if (stats[i].allocations > 0) allocated = true;
        }

        // Ensure memory was allocated via the Memory Manager
        STF_ASSERT_TRUE(allocated);
    }
}

STF_TEST(MemoryAllocator, TestVectorOfPackets)
{
    // Define the memory profile
    Terra::MemoryManager::MemoryProfile profile =
    {
        // Size, Minimum, Maximum, Excess Allowed
        {    64,       5,      10, true  },
        {   256,       2,      10, true  },
        {   512,       2,      10, true  },
        {  2048,       1,      20, true  },
        { 65536,       0,       1, true  }
    };

    // Create a Memory Manager for the given profile
    Terra::Logger::LoggerPointer logger = std::make_shared<Terra::Logger::Logger>(std::cout);
    Terra::MemoryManager::MemoryManagerPointer memory_manager =
        std::make_shared<Terra::MemoryManager::MemoryManager>(profile, logger);

    {
        // Get the statistics
        auto stats = memory_manager->GetStatistics();

        // There should be 5 sets of statistics, one for each descriptor
        STF_ASSERT_EQ(5, stats.size());

        // Iterate over each statistic and check expected values
        for (std::size_t i = 0; i < stats.size(); i++)
        {
            STF_ASSERT_EQ(profile[i].size, stats[i].size);
            STF_ASSERT_EQ(0, stats[i].allocations);
            STF_ASSERT_EQ(0, stats[i].deallocations);
            STF_ASSERT_EQ(0, stats[i].corruption_count);
            STF_ASSERT_EQ(0, stats[i].outstanding);
            STF_ASSERT_EQ(0, stats[i].max_outstanding);
            STF_ASSERT_EQ(0, stats[i].unfulfilled);
        }
    }

    {
        using DataBuffer =
            std::vector<std::uint8_t,
                        Terra::MemoryManager::MemoryAllocator<std::uint8_t>>;
        std::vector<DataBuffer,
                    Terra::MemoryManager::MemoryAllocator<DataBuffer>>
            vector(memory_manager);

        for (std::size_t i = 0; i < 10; i++)
        {
            // Create a data buffer
            DataBuffer buffer(1500, memory_manager);

            // Store a data buffer of size 1500 in the vector
            vector.push_back(std::move(buffer));
        }
    }

    {
        // Get the statistics
        auto stats = memory_manager->GetStatistics();

        // There should be 5 sets of statistics, one for each descriptor
        STF_ASSERT_EQ(5, stats.size());

        // Was memory allocated in the Memory Manager?
        bool allocated = false;

        // Iterate over each statistic and check expected values
        for (std::size_t i = 0; i < stats.size(); i++)
        {
            STF_ASSERT_EQ(profile[i].size, stats[i].size);
            STF_ASSERT_EQ(stats[i].allocations, stats[i].deallocations);
            STF_ASSERT_EQ(0, stats[i].corruption_count);
            STF_ASSERT_EQ(0, stats[i].outstanding);
            STF_ASSERT_EQ(0, stats[i].unfulfilled);
            if (stats[i].allocations > 0) allocated = true;
        }

        // Ensure memory was allocated via the Memory Manager
        STF_ASSERT_TRUE(allocated);
    }
}
