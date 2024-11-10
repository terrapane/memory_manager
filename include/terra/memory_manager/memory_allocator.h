/*
 *  memory_allocator.h
 *
 *  Copyright (C) 2024
 *  Terrapane Corporation
 *  All Rights Reserved
 *
 *  Author:
 *      Paul E. Jones <paulej@packetizer.com>
 *
 *  Description:
 *      This object defines an allocator that will utilize the MemoryManager
 *      to allocate and free memory.  It is intended for use with STL containers
 *      and such that take an allocator as an argument.  For example, the
 *      following defines a vector of integers that uses the MemoryManager.
 *
 *          std::vector<int, MemoryAllocator<int>> my_vector(memory_manager);
 *
 *      The primary benefits of using the MemoryAllocator is to reduce memory
 *      fragmentation and increase performance. However, it's important to
 *      understand that the global MemoryManager created for use with
 *      MemoryAllocator will never free memory.
 *
 *  Portability Issues:
 *      None.
 */

#pragma once

#include <cstdlib>
#include <cstddef>
#include <limits>
#include <new>
#include "memory_manager.h"

namespace Terra::MemoryManager
{

// Define the MemoryAllocator class
template<typename T>
class MemoryAllocator
{
    public:
        // Required type specification
        using value_type = T;

        // Default constructor
        constexpr MemoryAllocator(const MemoryManagerPointer &memory_manager) :
            memory_manager(memory_manager)
        {
        }

        // Trivial copy constructor
        template<typename U>
        constexpr MemoryAllocator(const MemoryAllocator<U> &other) noexcept :
            memory_manager(other.memory_memory_manager)
        {
        }

        // Default destructor
        constexpr ~MemoryAllocator() = default;

        /*
         *  MemoryAllocator::allocate()
         *
         *  Description:
         *      Allocates the specified number of type T items.
         *
         *  Parameters:
         *      n [in]
         *          Number of items of type T for which memory should be
         *          allocated.
         *
         *  Returns:
         *      A pointer to the allocated memory.
         *
         *  Comments:
         *      This function will throw an exception on failure.
         */
        [[nodiscard]] constexpr T *allocate(std::size_t n) const
        {
            // If the request is too large, throw an exception
            if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
            {
                throw std::bad_array_new_length();
            }

            // Attempt to allocate the requested memory (exception on failure)
            return static_cast<T *>(memory_manager->Allocate(sizeof(T) * n));
        }

        /*
         *  MemoryAllocator::deallocate()
         *
         *  Description:
         *      Free memory previously allocated by allocate().
         *
         *  Parameters:
         *      p [in]
         *          A pointer to the memory to be freed.
         *
         *      n [in]
         *          The number of items of type T that were previously allocated.
         *
         *  Returns:
         *      Nothing.
         *
         *  Comments:
         *      None.
         */
        constexpr void deallocate(T *p,
                                [[maybe_unused]] std::size_t n) const noexcept
        {
            // If the pointer is nullptr, just return
            if (p == nullptr) return;

            // Delete the previously allocated memory
            memory_manager->Free(p);
        }

        /*
         *  MemoryAllocator::operator==()
         *
         *  Description:
         *      Checks to see if memory allocated by one allocator can be freed
         *      by another allocator.
         *
         *  Parameters:
         *      other [in]
         *          A reference to the other allocator object.
         *
         *  Returns:
         *      Always returns true.
         *
         *  Comments:
         *      None.
         */
        template<typename U>
        constexpr bool operator==(const MemoryAllocator<U> &other) const noexcept
        {
            return memory_manager.get() == other.memory_manager.get();
        }

    protected:
        MemoryManagerPointer memory_manager;
};

} // namespace Terra::MemoryManager
