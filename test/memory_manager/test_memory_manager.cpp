/*
 *  test_memory_manager.cpp
 *
 *  Copyright (C) 2024
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Description:
 *      This module will test the MemoryManager
 *
 *  Portability Issues:
 *      None.
 */

#include <vector>
#include <cstring>
#include <terra/memory_manager/memory_manager.h>
#include <terra/stf/stf.h>

STF_TEST(MemMgr, Basic)
{
    // Define the memory profile
    Terra::MemoryManager::MemoryProfile profile =
    {
        // Size, Minimum, Maximum, Excess Allowed
        {    64,       5,      10, true    },
        {   256,       2,      10, true    },
        {   512,       2,      10, true    },
        {  1500,       1,      20, true    },
        { 65536,       0,       1, true    }
    };

    // Create a Memory Manager for the given profile
    Terra::MemoryManager::MemoryManager memory_manager(profile);

    // Get the statistics
    auto stats = memory_manager.GetStatistics();

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

STF_TEST(MemMgr, AllocationsExcess)
{
    std::vector<void *> allocations;

    // Define the memory profile
    Terra::MemoryManager::MemoryProfile profile =
    {
        // Size, Minimum, Maximum, Excess Allowed
        {    64,       5,      10, true    },
        {   256,       2,      10, true    },
        {   512,       2,      10, true    },
        {  1500,       1,      20, true    },
        { 65536,       0,       1, true    }
    };

    // Create a Memory Manager for the given profile
    Terra::MemoryManager::MemoryManager memory_manager(profile);

    // Allocate memory blocks in excess of the maximum
    for(unsigned i = 0; i < 20; i++)
    {
        void *p = memory_manager.Allocate(128);
        STF_ASSERT_NE(nullptr, p);
        allocations.push_back(p);
    }

    // Get the statistics
    auto stats = memory_manager.GetStatistics();

    // There should be 5 sets of statistics, one for each descriptor
    STF_ASSERT_EQ(5, stats.size());

    // Iterate over each statistic and check expected values
    for (std::size_t i = 0; i < stats.size(); i++)
    {
        STF_ASSERT_EQ(profile[i].size, stats[i].size);
        if (i == 1)
        {
            STF_ASSERT_EQ(20, stats[i].allocations);
            STF_ASSERT_EQ(20, stats[i].outstanding);
            STF_ASSERT_EQ(20, stats[i].max_outstanding);
        }
        else
        {
            STF_ASSERT_EQ(0, stats[i].allocations);
            STF_ASSERT_EQ(0, stats[i].outstanding);
            STF_ASSERT_EQ(0, stats[i].max_outstanding);
        }
        STF_ASSERT_EQ(0, stats[i].deallocations);
        STF_ASSERT_EQ(0, stats[i].corruption_count);
        STF_ASSERT_EQ(0, stats[i].unfulfilled);
    }

    // Now free all of the memory
    for (auto *p : allocations)
    {
        memory_manager.Free(p);
    }

    // Get the statistics again
    stats = memory_manager.GetStatistics();

    // Ensure all of the allocations were recorded
    for (std::size_t i = 0; i < stats.size(); i++)
    {
        STF_ASSERT_EQ(profile[i].size, stats[i].size);
        if (i == 1)
        {
            STF_ASSERT_EQ(20, stats[i].allocations);
            STF_ASSERT_EQ(20, stats[i].deallocations);
            STF_ASSERT_EQ(20, stats[i].max_outstanding);
        }
        else
        {
            STF_ASSERT_EQ(0, stats[i].allocations);
            STF_ASSERT_EQ(0, stats[i].deallocations);
            STF_ASSERT_EQ(0, stats[i].max_outstanding);
        }
        STF_ASSERT_EQ(0, stats[i].corruption_count);
        STF_ASSERT_EQ(0, stats[i].outstanding);
        STF_ASSERT_EQ(0, stats[i].unfulfilled);
    }
}

STF_TEST(MemMgr, AllocationsWithoutExcess)
{
    std::vector<void *> allocations;

    // Define the memory profile
    Terra::MemoryManager::MemoryProfile profile =
    {
        // Size, Minimum, Maximum, Excess Allowed
        {    64,       5,      10, false    },
        {   256,       2,      10, false    },
        {   512,       2,      10, false    },
        {  1500,       1,      20, false    },
        { 65536,       0,       1, false    }
    };

    // Create a Memory Manager for the given profile
    Terra::MemoryManager::MemoryManager memory_manager(profile);

    // Allocate memory blocks in excess of the maximum, additional allocations
    // will be pulled from the next Descriptor (i.e., 512 block)
    for(unsigned i = 0; i < 20; i++)
    {
        void *p = memory_manager.Allocate(128);
        STF_ASSERT_NE(nullptr, p);
        allocations.push_back(p);
    }

    // Get the statistics
    auto stats = memory_manager.GetStatistics();

    // There should be 5 sets of statistics, one for each descriptor
    STF_ASSERT_EQ(5, stats.size());

    // Iterate over each statistic and check expected values
    for (std::size_t i = 0; i < stats.size(); i++)
    {
        STF_ASSERT_EQ(profile[i].size, stats[i].size);
        if ((i == 1) || (i == 2))
        {
            STF_ASSERT_EQ(10, stats[i].allocations);
            STF_ASSERT_EQ(10, stats[i].outstanding);
            STF_ASSERT_EQ(10, stats[i].max_outstanding);
        }
        else
        {
            STF_ASSERT_EQ(0, stats[i].allocations);
            STF_ASSERT_EQ(0, stats[i].outstanding);
            STF_ASSERT_EQ(0, stats[i].max_outstanding);
        }
        STF_ASSERT_EQ(0, stats[i].deallocations);
        STF_ASSERT_EQ(0, stats[i].corruption_count);
        if (i == 1)
        {
            STF_ASSERT_EQ(10, stats[i].unfulfilled);
        }
        else
        {
            STF_ASSERT_EQ(0, stats[i].unfulfilled);
        }
    }

    // Now free all of the memory
    for (auto *p : allocations)
    {
        memory_manager.Free(p);
    }

    // Get the statistics again
    stats = memory_manager.GetStatistics();

    // Ensure all of the allocations were recorded
    for (std::size_t i = 0; i < stats.size(); i++)
    {
        STF_ASSERT_EQ(profile[i].size, stats[i].size);
        if ((i == 1) || (i == 2))
        {
            STF_ASSERT_EQ(10, stats[i].allocations);
            STF_ASSERT_EQ(10, stats[i].deallocations);
            STF_ASSERT_EQ(10, stats[i].max_outstanding);
        }
        else
        {
            STF_ASSERT_EQ(0, stats[i].allocations);
            STF_ASSERT_EQ(0, stats[i].deallocations);
            STF_ASSERT_EQ(0, stats[i].max_outstanding);
        }
        STF_ASSERT_EQ(0, stats[i].corruption_count);
        STF_ASSERT_EQ(0, stats[i].outstanding);
        if (i == 1)
        {
            STF_ASSERT_EQ(10, stats[i].unfulfilled);
        }
        else
        {
            STF_ASSERT_EQ(0, stats[i].unfulfilled);
        }
    }
}

STF_TEST(MemMgr, Corruption)
{
    // Define the memory profile
    Terra::MemoryManager::MemoryProfile profile =
    {
        // Size, Minimum, Maximum, Excess Allowed
        {    64,       5,      10, true    },
        {   256,       2,      10, true    },
        {   512,       2,      10, true    },
        {  1500,       1,      20, true    },
        { 65536,       0,       1, true    }
    };

    // Create a Memory Manager for the given profile
    Terra::MemoryManager::MemoryManager memory_manager(profile);

    // Allocate memory
    void *p = memory_manager.Allocate(128);
    STF_ASSERT_NE(nullptr, p);

    // Get the statistics
    auto stats = memory_manager.GetStatistics();

    // There should be 5 sets of statistics, one for each descriptor
    STF_ASSERT_EQ(5, stats.size());

    // Iterate over each statistic and check expected values
    for (std::size_t i = 0; i < stats.size(); i++)
    {
        STF_ASSERT_EQ(profile[i].size, stats[i].size);
        if (i == 1)
        {
            STF_ASSERT_EQ(1, stats[i].allocations);
            STF_ASSERT_EQ(1, stats[i].outstanding);
            STF_ASSERT_EQ(1, stats[i].max_outstanding);
            STF_ASSERT_EQ(0, stats[i].corruption_count);
        }
        else
        {
            STF_ASSERT_EQ(0, stats[i].allocations);
            STF_ASSERT_EQ(0, stats[i].outstanding);
            STF_ASSERT_EQ(0, stats[i].max_outstanding);
            STF_ASSERT_EQ(0, stats[i].corruption_count);
        }
        STF_ASSERT_EQ(0, stats[i].deallocations);
        STF_ASSERT_EQ(0, stats[i].unfulfilled);
    }

    // Corrupt the block -- specifically, overrun the end of the block
    std::memset(p, 0, 257);

    // Free the memory
    memory_manager.Free(p);

    // Get the statistics again
    stats = memory_manager.GetStatistics();

    // Ensure all of the allocations were recorded
    for (std::size_t i = 0; i < stats.size(); i++)
    {
        STF_ASSERT_EQ(profile[i].size, stats[i].size);
        if (i == 1)
        {
            STF_ASSERT_EQ(1, stats[i].allocations);
            STF_ASSERT_EQ(1, stats[i].deallocations);
            STF_ASSERT_EQ(1, stats[i].max_outstanding);
            STF_ASSERT_EQ(1, stats[i].corruption_count);
        }
        else
        {
            STF_ASSERT_EQ(0, stats[i].allocations);
            STF_ASSERT_EQ(0, stats[i].deallocations);
            STF_ASSERT_EQ(0, stats[i].max_outstanding);
            STF_ASSERT_EQ(0, stats[i].corruption_count);
        }
        STF_ASSERT_EQ(0, stats[i].outstanding);
        STF_ASSERT_EQ(0, stats[i].unfulfilled);
    }
}

STF_TEST(MemMgr, Exhaustion)
{
    std::vector<void *> allocations;

    // Define the memory profile
    Terra::MemoryManager::MemoryProfile profile =
    {
        // Size, Minimum, Maximum, Excess Allowed
        {    64,       5,       5, false   },
        {   256,       2,       5, false   }
    };

    // Create a Memory Manager for the given profile
    Terra::MemoryManager::MemoryManager memory_manager(profile);

    // Allocate memory blocks in excess of the maximum, additional allocations
    // will be pulled from the next Descriptor (i.e., 256 block)
    for(unsigned i = 0; i < 20; i++)
    {
        void *p = memory_manager.Allocate(32);
        if (i < 10)
        {
            STF_ASSERT_NE(nullptr, p);
            allocations.push_back(p);
        }
        else
        {
            STF_ASSERT_EQ(nullptr, p);
        }
    }

    // Get the statistics
    auto stats = memory_manager.GetStatistics();

    // There should be 5 sets of statistics, one for each descriptor
    STF_ASSERT_EQ(2, stats.size());

    // Iterate over each statistic and check expected values
    for (std::size_t i = 0; i < stats.size(); i++)
    {
        STF_ASSERT_EQ(profile[i].size, stats[i].size);
        STF_ASSERT_EQ(5, stats[i].allocations);
        STF_ASSERT_EQ(5, stats[i].outstanding);
        STF_ASSERT_EQ(5, stats[i].max_outstanding);
        STF_ASSERT_EQ(0, stats[i].deallocations);
        STF_ASSERT_EQ(0, stats[i].corruption_count);
        if (i == 0)
        {
            STF_ASSERT_EQ(15, stats[i].unfulfilled);
        }
        else
        {
            STF_ASSERT_EQ(10, stats[i].unfulfilled);
        }
    }

    // Now free all of the memory
    for (auto *p : allocations)
    {
        memory_manager.Free(p);
    }

    // Get the statistics again
    stats = memory_manager.GetStatistics();

    // Ensure all of the allocations were recorded
    for (std::size_t i = 0; i < stats.size(); i++)
    {
        STF_ASSERT_EQ(profile[i].size, stats[i].size);
        STF_ASSERT_EQ(5, stats[i].allocations);
        STF_ASSERT_EQ(0, stats[i].outstanding);
        STF_ASSERT_EQ(5, stats[i].max_outstanding);
        STF_ASSERT_EQ(5, stats[i].deallocations);
        STF_ASSERT_EQ(0, stats[i].corruption_count);
        if (i == 0)
        {
            STF_ASSERT_EQ(15, stats[i].unfulfilled);
        }
        else
        {
            STF_ASSERT_EQ(10, stats[i].unfulfilled);
        }
    }
}

STF_TEST(MemMgr, ManyAllocateFree)
{
    // Define the memory profile
    Terra::MemoryManager::MemoryProfile profile =
    {
        // Size, Minimum, Maximum, Excess Allowed
        {    64,       5,      10, true    },
        {   256,       2,      10, true    },
        {   512,       2,      10, true    },
        {  1500,       1,      20, true    },
        { 65536,       0,       1, true    }
    };

    // Create a Memory Manager for the given profile
    Terra::MemoryManager::MemoryManager memory_manager(profile);

    // Repeatedly allocate and free memory
    for(unsigned i = 0; i < 250; i++)
    {
        void *p = memory_manager.Allocate(256);
        STF_ASSERT_NE(nullptr, p);
        std::memset(p, 0, 256);
        memory_manager.Free(p);
    }

    // Get the statistics
    auto stats = memory_manager.GetStatistics();

    // There should be 5 sets of statistics, one for each descriptor
    STF_ASSERT_EQ(5, stats.size());

    // Iterate over each statistic and check expected values
    for (std::size_t i = 0; i < stats.size(); i++)
    {
        STF_ASSERT_EQ(profile[i].size, stats[i].size);
        if (i == 1)
        {
            STF_ASSERT_EQ(250, stats[i].allocations);
            STF_ASSERT_EQ(0, stats[i].outstanding);
            STF_ASSERT_EQ(1, stats[i].max_outstanding);
            STF_ASSERT_EQ(250, stats[i].deallocations);
        }
        else
        {
            STF_ASSERT_EQ(0, stats[i].allocations);
            STF_ASSERT_EQ(0, stats[i].outstanding);
            STF_ASSERT_EQ(0, stats[i].max_outstanding);
            STF_ASSERT_EQ(0, stats[i].deallocations);
        }
        STF_ASSERT_EQ(0, stats[i].corruption_count);
        STF_ASSERT_EQ(0, stats[i].unfulfilled);
    }
}

STF_TEST(MemMgr, ManyAllocateFree2)
{
    // Define the memory profile
    Terra::MemoryManager::MemoryProfile profile =
    {
        // Size, Minimum, Maximum, Excess Allowed
        {    64,       5,      10, true    },
        {   256,       2,      10, true    },
        {   512,       2,      10, true    },
        {  1500,       1,      20, true    },
        { 65536,       0,       1, true    }
    };

    // Create a Memory Manager for the given profile
    Terra::MemoryManager::MemoryManager memory_manager(profile);

    // Repeatedly allocate and free memory
    for(unsigned i = 0; i < 250; i++)
    {
        void *p = memory_manager.Allocate(256);
        void *q = memory_manager.Allocate(256);
        STF_ASSERT_NE(nullptr, p);
        STF_ASSERT_NE(nullptr, q);
        std::memset(p, 0, 256);
        std::memset(q, 0, 256);
        memory_manager.Free(p);
        memory_manager.Free(q);
    }

    // Get the statistics
    auto stats = memory_manager.GetStatistics();

    // There should be 5 sets of statistics, one for each descriptor
    STF_ASSERT_EQ(5, stats.size());

    // Iterate over each statistic and check expected values
    for (std::size_t i = 0; i < stats.size(); i++)
    {
        STF_ASSERT_EQ(profile[i].size, stats[i].size);
        if (i == 1)
        {
            STF_ASSERT_EQ(500, stats[i].allocations);
            STF_ASSERT_EQ(0, stats[i].outstanding);
            STF_ASSERT_EQ(2, stats[i].max_outstanding);
            STF_ASSERT_EQ(500, stats[i].deallocations);
        }
        else
        {
            STF_ASSERT_EQ(0, stats[i].allocations);
            STF_ASSERT_EQ(0, stats[i].outstanding);
            STF_ASSERT_EQ(0, stats[i].max_outstanding);
            STF_ASSERT_EQ(0, stats[i].deallocations);
        }
        STF_ASSERT_EQ(0, stats[i].corruption_count);
        STF_ASSERT_EQ(0, stats[i].unfulfilled);
    }
}
