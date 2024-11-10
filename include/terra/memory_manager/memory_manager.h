/*
 *  memory_manager.h
 *
 *  Copyright (C) 2024
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Description:
 *      This file defines the MemoryManager object and related structures.
 *
 *      The purpose of the Memory Manager is to allow an application to
 *      pre-allocated chunks of memory for operations that use certain size
 *      memory chunks repetitively.  For example, server processes that allocate
 *      and free packet buffers repeatedly can benefit from using the
 *      Memory Manager.  With the Memory Manager, allocations would be faster,
 *      since allocating and freeing memory chunks are really just a matter of
 *      moving a pointer.
 *
 *      An application may instantiate a single MemoryManager or multiple
 *      MemoryManager objects.  The choice is really for the developer, though
 *      it makes most sense to use a different one for memory allocations that
 *      follow a different profile.  For example, a potion of code that
 *      constantly allocates and frees 1500 octets for data packets should
 *      probably have a MemoryManager object dedicated to that task, while
 *      a different one might be used for allocating buffers to process
 *      received data (e.g., aggregation, decoding, or decrypting).
 *
 *      It is important is that the same Memory Manager that allocates a
 *      given chunk of memory be the same one used to free that memory.
 *
 *      When initializing the Memory Manager, the developer provides a
 *      MemoryProfile to the constructor.  The profile defines the various
 *      allocation sizes and number of allocations to make of that size.
 *      This information is encapsulated in the MemoryDescriptor structure.
 *      For each descriptor, there is an initial number of allocations to make
 *      and a maximum number that will be retained after free.  If the maximum
 *      number is 0, that means there is no limit on how much memory will be
 *      held.  If the minimum number is zero, it means no memory
 *      for that descriptor will be pre-allocated.
 *
 *      Below is an example profile definition with various block sizes
 *      for each descriptor, the minimum number to pre-allocate, the maximum
 *      that will be retained (and not returned to the heap), and a flag
 *      to indicate whether additional heap allocations are allowed or if
 *      requests for memory should just fail if the maximum allocations have
 *      been made.
 *
 *          Terra::MemoryManager::MemoryProfile profile =
 *          {
 *              // Size, Minimum, Maximum, Excess Allowed
 *              {    64,       5,      10, true  },
 *              {   256,       2,      10, true  },
 *              {   512,       2,      10, true  },
 *              {  1500,       1,      20, true  },
 *              { 65535,       0,      10, false }
 *          };
 *
 *      When memory if freed, the memory chunks are placed back into the
 *      appropriate vector automatically.  This is made possible via special
 *      header placed at the start of the memory.  If additional allocations
 *      were necessary from the heap, they will be returned retained until
 *      the maximum number of allocations is reached.  Any beyond that size
 *      will be returned to the heap.  (Ideally, one should perform tests
 *      to determine how many allocations a system will use in practice to
 *      avoid any frees to the heap, as such frees obviates the benefits gained
 *      by using the Memory Manager.)
 *
 *      Sometimes, there may be a need to allocate memory beyond the specified
 *      maximum (if there is one).  In the case where a maximum is specified,
 *      one may indicate allowing excess allocations from the heap or not.  If
 *      excess is not allowed, the user will receive a nullptr when the maximum
 *      is exceeded to indicate the allocation failed, just as it would if
 *      allocation from the heap that fails will return a nullptr.
 *
 *      If the maximum value is 0, that indicates there is no limit to
 *      allocations for that descriptor.  As such, when the maximum is 0 the
 *      "excess allowed" field is ignored.
 *
 *      For each descriptor, if the maximum value is less than the minimum
 *      value, the maximum value will be set equal to the minimum value.  The
 *      exception is when the maximum value is zero, which indicates no maximum.
 *
 *      When calling Allocate(), the first descriptor with a size value large
 *      enough to satisfy the request will be used.
 *
 *  Portability Issues:
 *      None.
 */

#pragma once

#include <cstdlib>
#include <cstdint>
#include <vector>
#include <utility>
#include <memory>
#include <mutex>
#include <terra/logger/logger.h>

namespace Terra::MemoryManager
{

// Define a MemoryDescriptor structure
struct MemoryDescriptor
{
    std::size_t size;                           // Size of memory blocks
    std::size_t minimum;                        // Minimum to preallocate
    std::size_t maximum;                        // Maximum to retain available
    bool excess_allowed;                        // Allow excess heap allocations
};

// Define a structure to hold various statistics per bucket
struct Statistics
{
    std::size_t size;                           // Size of memory blocks
    std::uint64_t allocations;                  // User allocations
    std::uint64_t deallocations;                // User deallocations
    std::uint64_t corruption_count;             // Corrupt block count
    std::uint64_t max_outstanding;              // Maximum blocks outstanding
    std::uint64_t outstanding;                  // Blocks outstanding
    std::uint64_t unfulfilled;                  // Allocations unfulfilled
};

// Define the MemoryProfile type
using MemoryProfile = std::vector<MemoryDescriptor>;

// Define the MemoryManager object
class MemoryManager
{
    public:
        MemoryManager(MemoryProfile profile,
                      const Logger::LoggerPointer &parent_logger = {},
                      bool log_statistics = true);
        ~MemoryManager();

        void *Allocate(std::size_t size);
        bool Free(void *p);
        std::vector<Statistics> GetStatistics() const;

    protected:
        bool PerformAllocation(std::size_t index);

        MemoryProfile profile;
        Logger::LoggerPointer logger;
        bool log_statistics;
        std::vector<std::vector<uint8_t *>> allocations;
        std::vector<Statistics> statistics;
        mutable std::mutex mutex;
};

// Define a shared pointer type
using MemoryManagerPointer = std::shared_ptr<MemoryManager>;

} // namespace Terra::MemoryManager
