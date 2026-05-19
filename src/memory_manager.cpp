/*
 *  memory_manager.cpp
 *
 *  Copyright (C) 2024, 2026
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Description:
 *      This file implements the MemoryManager object. The structure of an
 *      allocated memory block is as follows:
 *          [MemoryHeader]  - For bookkeeping and corruption detection
 *          [Data]          - Pointer given by Allocate()
 *          [MemoryTrailer] - For corruption detection
 *
 *  Portability Issues:
 *      None.
 */

#include <algorithm>
#include <new>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>
#include <memory>
#include <iostream>
#include <mutex>
#include <vector>
#include <terra/memory_manager/memory_manager.h>
#include <terra/logger/logger.h>

namespace Terra::MemoryManager
{

namespace
{

// Define memory alignment
#ifdef __cpp_lib_hardware_interference_size
    constexpr std::size_t Allocation_Alignment =
        std::hardware_destructive_interference_size;
#else
    constexpr std::size_t Allocation_Alignment = alignof(std::max_align_t);
#endif

// Disable MSVC warning "Structure was padded due to alignment specifier"
#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4324)
#endif

// Header placed at the start of allocated memory
// NOLINTNEXTLINE(altera-struct-pack-align)
struct alignas(Allocation_Alignment) MemoryHeader
{
    MemoryManager *memory_manager;              // Pointer to owning object
    std::size_t index;                          // Profile index
    std::uint64_t marker;                       // Head identifier
};

#ifdef _MSC_VER
    #pragma warning(pop)
#endif

// Trailer placed at the end of allocated memory
struct MemoryTrailer
{
    std::uint64_t marker;                       // Trailer identifier
};

// Define the header and tailer marker values
constexpr std::uint64_t Header_Marker_Value = 0xC1F03D8B4A725378;
constexpr std::uint64_t Trailer_Marker_Value = 0x215F8A1A6853658B;

// Allocate memory block
inline std::uint8_t *AllocateBlock(std::size_t block_size)
{
    auto total_size = sizeof(MemoryHeader) + block_size;
    auto remainder = total_size % Allocation_Alignment;
    if (remainder != 0) total_size += (Allocation_Alignment - remainder);

    // The trailer is aligned to the Allocation_Alignment boundary since
    // MemoryHeader is aligned and the allocation size is inflated, if
    // necessary, to include padding to facilitate alignment
    total_size += sizeof(MemoryTrailer);

    void *block = ::operator new[](total_size,
                                   std::align_val_t{Allocation_Alignment},
                                   std::nothrow);

    return reinterpret_cast<std::uint8_t *>(block);
}

// Function to delete memory block
inline void DeleteBlock(std::uint8_t *block)
{
    ::operator delete[](block, std::align_val_t{Allocation_Alignment});
}

// Helper function to do type casting
constexpr auto PointerDiff(std::size_t distance)
{
    using DiffType = std::iterator_traits<std::uint8_t *>::difference_type;
    return static_cast<DiffType>(distance);
}

// Helper to return a pointer to the MemoryHeader structure for this data
constexpr std::uint8_t *GetHeaderPointer(std::uint8_t *data)
{
    return std::prev(data, PointerDiff(sizeof(MemoryHeader)));
}

// Helper to return a pointer to the user "data" block after the header
constexpr std::uint8_t *GetDataPointer(std::uint8_t *block)
{
    return std::next(block, PointerDiff(sizeof(MemoryHeader)));
}

// Helper to return a pointer to the memory block trailer
constexpr std::uint8_t *GetTrailerPointer(std::uint8_t *block,
                                          std::size_t block_size)
{
    auto total_size =
        sizeof(MemoryHeader) + block_size;
    auto remainder = total_size % Allocation_Alignment;
    if (remainder != 0) total_size += (Allocation_Alignment - remainder);

    return std::next(block, PointerDiff(total_size));
}

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
    std::ranges::sort(
        this->profile,
        [](const MemoryDescriptor &a, const MemoryDescriptor &b) -> bool
        { return a.size < b.size; });

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
            DeleteBlock(block);
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
    const std::lock_guard<std::mutex> lock(mutex);

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

            // Return a pointer to the data just after the MemoryHeader
            return GetDataPointer(block);
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
 *      the block will be freed to the heap.
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
    const std::lock_guard<std::mutex> lock(mutex);

    // Get access to the MemoryHeader location (same as block location)
    std::uint8_t *block = GetHeaderPointer(reinterpret_cast<std::uint8_t *>(p));
    const MemoryHeader *header = reinterpret_cast<MemoryHeader *>(block);

    // Ensure that memory block is valid
    if (block == nullptr)
    {
        logger->error << "The pointer given does not appear to be valid"
                      << std::flush;
        return false;
    }

    // Verify that memory appears to belong to this Memory Manager
    if (header->memory_manager != this)
    {
        logger->error << "Attempt to free memory not allocated with this "
                         "Memory Manager object"
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
        DeleteBlock(block);
        return true;
    }

    // Get the pointer to the memory block trailer
    const MemoryTrailer *trailer = reinterpret_cast<MemoryTrailer *>(
        GetTrailerPointer(block, profile[header->index].size));

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
        DeleteBlock(block);
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
        DeleteBlock(block);
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
    const std::lock_guard<std::mutex> lock(mutex);

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
    std::uint8_t *block = AllocateBlock(profile[index].size);
    if (block == nullptr)
    {
        logger->error << "Failed to allocate heap memory" << std::flush;
        return false;
    }

    // Populate the header
    MemoryHeader *header = reinterpret_cast<MemoryHeader *>(block);
    (*header) = {};
    header->memory_manager = this;
    header->index = index;
    header->marker = Header_Marker_Value;

    // Populate the trailer
    MemoryTrailer *trailer = reinterpret_cast<MemoryTrailer *>(
            GetTrailerPointer(block, profile[header->index].size));
    trailer->marker = Trailer_Marker_Value;

    // Place the allocated memory into the deque
    allocations[index].push_back(block);

    return true;
}

} // namespace Terra::MemoryManager
