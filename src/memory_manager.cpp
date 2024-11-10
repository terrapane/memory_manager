/*
 *  memory_manager.cpp
 *
 *  Copyright (C) 2024
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Description:
 *      This file implements the MemoryManager object.
 *
 *  Portability Issues:
 *      None.
 */

#include <algorithm>
#include <terra/memory_manager/memory_manager.h>

namespace Terra::MemoryManager
{

namespace
{

// Header placed at the start of allocated memory
struct MemoryHeader
{
    MemoryManager *memory_manager;              // Pointer to owning object
    std::size_t index;                          // Profile index
    std::uint64_t marker;                       // Head identifier
};

// Trailer placed at the end of allocated memory
struct MemoryTrailer
{
    std::uint64_t marker;                       // Tail identifier
};

// Define the head and tail values
constexpr std::uint64_t Header_Marker_Value = 0xC1F03D8B4A725378;
constexpr std::uint64_t Trailer_Marker_Value = 0x215F8A1A6853658B;

} // namespace

/*
 *  MemoryManager::MemoryManager()
 *
 *  Description:
 *      Constructor for the MemoryManager object.
 *
 *  Parameters:
 *      profile [in]
 *          The MemoryProfile to use with this MemoryManager.
 *
 *  Returns:
 *      profile [in]
 *          The memory profile that contains a vector of MemoryDescriptor
 *          structures that will be used by the Memory Manager.
 *
 *      parent_logger [in]
 *          An optional Logger object to which logging output will be directed.
 *
 *      log_statistics [in]
 *          Log usage statistics on destruction.
 *
 *  Comments:
 *      None.
 */
MemoryManager::MemoryManager(MemoryProfile profile,
                             const Logger::LoggerPointer &parent_logger,
                             bool log_statistics) :
    profile{std::move(profile)},
    logger{std::make_shared<Logger::Logger>(parent_logger, "MMGR")},
    log_statistics{log_statistics}
{
    logger->info << "Initializing memory profiles" << std::flush;

    // Sort the profile deque according to size
    std::sort(this->profile.begin(),
              this->profile.end(),
              [](const MemoryDescriptor &a, const MemoryDescriptor &b) -> bool
              {
                  return a.size < b.size;
              });

    // Allocate memory
    for (std::size_t index = 0; index < this->profile.size(); index++)
    {
        // If maximum is less than minumum, make them equal
        if ((this->profile[index].maximum != 0) &&
            (this->profile[index].maximum < this->profile[index].minimum))
        {
            logger->warning << "Descriptor size " << this->profile[index].size
                            << " has an invalid maximum value" << std::flush;
            this->profile[index].maximum = this->profile[index].minimum;
        }

        // Create a zero-initialized Statistics structure for this profile entry
        statistics.emplace_back(Statistics{});
        statistics.back().size = this->profile[index].size;

        // Create an empty allocations element
        allocations.emplace_back();

        logger->info << "Descriptor size " << this->profile[index].size
                     << ", count " << this->profile[index].minimum
                     << std::flush;

        // Allocate the requested number of blocks
        for (std::size_t i = 0; i < this->profile[index].minimum; i++)
        {
            PerformAllocation(index);
        }
    }
}

/*
 *  MemoryManager::~MemoryManager()
 *
 *  Description:
 *      Allocate a block of memory for the given profile index.
 *
 *  Parameters:
 *      Nome.
 *
 *  Returns:
 *      Nothing.
 *
 *  Comments:
 *      The mutex MUST be locked by the calling function.
 */
MemoryManager::~MemoryManager()
{
    logger->info << "Memory Manager Usage Statistics" << std::flush;

    // Iterate over each descriptor in the profile
    for (std::size_t index = 0; index < profile.size(); index++)
    {
        if (log_statistics)
        {
            logger->info << "  Block size: " << profile[index].size
                         << std::flush;
            logger->info << "    Allocations: " << statistics[index].allocations
                         << std::flush;
            logger->info << "    Deallocations: "
                         << statistics[index].deallocations << std::flush;
            logger->info << "    Corrupted: "
                         << statistics[index].corruption_count << std::flush;
            logger->info << "    Max Outstanding: "
                         << statistics[index].max_outstanding << std::flush;
            logger->info << "    Outstanding: " << statistics[index].outstanding
                         << std::flush;
            logger->info << "    Unfulfilled: " << statistics[index].unfulfilled
                         << std::flush;
        }

        // Free all allocated memory in the deque
        while (!allocations[index].empty())
        {
            std::uint8_t *block = allocations[index].back();
            allocations[index].pop_back();
            delete[] block;
        }
    }
}

/*
 *  MemoryManager::Allocate()
 *
 *  Description:
 *      This function will allocate memory of the requested size.  It does this
 *      by searching through the MemoryProfile looking for a descriptor that
 *      is for memory blocks large enough to satisfy the request.  It will then
 *      return a block from that part of the profile, if the request can be
 *      satisfied.  If it cannot, it will move to the next (larger) descriptor
 *      and attempt the request again.
 *
 *  Parameters:
 *      size [in]
 *          The size of the memory requested.
 *
 *  Returns:
 *      A pointer to a block of memory the user may use or nullptr if the
 *      request could not be satisfied.
 *
 *  Comments:
 *      None.
 */
void *MemoryManager::Allocate(std::size_t size)
{
    // Lock the mutex
    std::lock_guard<std::mutex> lock(mutex);

    // Iterate over each descriptor in the profile for available memory
    for (std::size_t index = 0; index < profile.size(); index++)
    {
        // If the descriptor indicates memory is too small, keep looking
        if (profile[index].size < size) continue;

        // If no memory blocks are available and allocation fails, keep looking
        if (allocations[index].empty() && !PerformAllocation(index))
        {
            // Note a fulfillment attempt failed
            statistics[index].unfulfilled++;
            continue;
        }

        // There should be a memory block, but double-check out of paranoia
        if (!allocations[index].empty())
        {
            // Update various statistics
            statistics[index].allocations++;
            statistics[index].outstanding++;
            statistics[index].max_outstanding =
                std::max(statistics[index].outstanding,
                         statistics[index].max_outstanding);

            // Grab a memory block off the back
            uint8_t *block = allocations[index].back();
            allocations[index].pop_back();

            // Return a pointer just after the MemoryHeader structure
            return reinterpret_cast<void *>(block + sizeof(MemoryHeader));
        }
    }

    return nullptr;
}

/*
 *  MemoryManager::Free()
 *
 *  Description:
 *      This function will free a block of memory.  Internally, the pointer
 *      will just be returned to the vector of allocated memory blocks.
 *      If, however, the vector of pointers exceed the maximum number then
 *      the buffer will be freed to the heap.
 *
 *  Parameters:
 *      p [in]
 *          Pointer to a block of memory provided by Allocate().
 *
 *  Returns:
 *      True if memory is freed, false if not.  The only reason false will
 *      be returned is if the pointer given does not belong to this Memory
 *      Manager.  This could happen if memory is corrupted.
 *
 *  Comments:
 *      It is important that the pointer provided actually be allocated by
 *      the Memory Manager, else this function might cause an access violation
 *      or memory leak.
 */
bool MemoryManager::Free(void *p)
{
    // Does the memory block appear bad?
    bool bad_block = false;

    // Lock the mutex
    std::lock_guard<std::mutex> lock(mutex);

    // Re-interpret the pointer for convenience
    std::uint8_t *block =
        reinterpret_cast<std::uint8_t *>(p) - sizeof(MemoryHeader);
    MemoryHeader *header = reinterpret_cast<MemoryHeader *>(block);

    // Ensure that memory block is not null and did not wrap
    if ((block == nullptr) || (block > reinterpret_cast<std::uint8_t *>(p)))
    {
        logger->error << "The pointer given does not appear to be valid"
                      << std::flush;
        return false;
    }

    // Verify that memory appears to belong to this Memory Manager
    if (header->memory_manager != this)
    {
        logger->error << "Attempt to free memory not allocated by this Memory "
                         "Manager object"
                      << std::flush;
        return false;
    }

    // Verify header marker
    if (header->marker != Header_Marker_Value) bad_block = true;

    // Check that the block index is within a valid range
    if (!profile.empty() && (header->index > profile.size() - 1))
    {
        logger->error << "Free request made, but the descriptor data is bad; "
                         "memory will be freed to the heap" << std::flush;
        delete[] block;
        return true;
    }

    // Get the pointer to the buffer trailer
    MemoryTrailer *trailer = reinterpret_cast<MemoryTrailer *>(
        block + sizeof(MemoryHeader) + profile[header->index].size);

    // Check trailer marker to ensure memory is not corrupt
    if (trailer->marker != Trailer_Marker_Value) bad_block = true;

    // Update statistics
    statistics[header->index].deallocations++;
    if (statistics[header->index].outstanding > 0)
    {
        statistics[header->index].outstanding--;
    }

    // If the block is bad, update statistics, delete, and return to heap
    if (bad_block)
    {
        statistics[header->index].corruption_count++;
        delete[] block;
        return true;
    }

    // Put the block back on the vector, if possible else free to the heap
    if ((profile[header->index].maximum == 0) ||
        (allocations[header->index].size() < profile[header->index].maximum))
    {
        allocations[header->index].push_back(block);
    }
    else
    {
        delete[] block;
    }

    return true;
}

/*
 *  MemoryManager::GetStatistics()
 *
 *  Description:
 *      Get the current Memory Manager statistics.
 *
 *  Parameters:
 *      None.
 *
 *  Returns:
 *      The Memory Manager Statistics structure containing the current
 *      statistics information.
 *
 *  Comments:
 *      None.
 */
std::vector<Statistics> MemoryManager::GetStatistics() const
{
    std::lock_guard<std::mutex> lock(mutex);

    return statistics;
}

/*
 *  MemoryManager::PerformAllocation()
 *
 *  Description:
 *      Allocate a block of memory for the given profile index.
 *
 *  Parameters:
 *      index [in]
 *          The index into the profile vector for which a memory allocation
 *          is to be made.
 *
 *  Returns:
 *      True if successful, false if not.
 *
 *  Comments:
 *      The mutex MUST be locked by the calling function.
 */
bool MemoryManager::PerformAllocation(std::size_t index)
{
    // Perform no allocation if beyond constraints
    if ((profile[index].maximum != 0) && (!profile[index].excess_allowed) &&
        ((allocations[index].size() + statistics[index].outstanding) >=
         profile[index].maximum))
    {
        return false;
    }

    // Allocate the requested memory block
    std::uint8_t *block =
        new std::uint8_t[sizeof(MemoryHeader) + profile[index].size +
                         sizeof(MemoryTrailer)];
    if (block == nullptr)
    {
        logger->error << "Failed to allocate heap memory" << std::flush;
        return false;
    }

    // Populate the header
    MemoryHeader *header = reinterpret_cast<MemoryHeader *>(block);
    header->memory_manager = this;
    header->index = index;
    header->marker = Header_Marker_Value;

    // Populate the trailer
    MemoryTrailer *trailer = reinterpret_cast<MemoryTrailer *>(
        block + sizeof(MemoryHeader) + profile[index].size);
    trailer->marker = Trailer_Marker_Value;

    // Place the allocated memory into the deque
    allocations[index].push_back(block);

    return true;
}

} // namespace Terra::MemoryManager
