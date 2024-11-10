# Memory Manager Library

## Memory Manager

This library contains a Memory Manager that can be used to preallocate memory.
This is useful for systems like many real-time communication systems that
repeatedly allocate and free memory.  Using the Memory Manager will reduce
memory fragmentation and improve performance.

To use the Memory Manager, one creates a Memory Profile containing a series
of Memory Descriptors and provide that during construction.  That would look
something like this:

```cpp
    // Define the memory profile
    Terra::MemoryManager::MemoryProfile profile =
    {
        // Size, Minimum, Maximum, Excess Allowed
        {    64,       5,      10, true  },
        {   256,       2,      10, true  },
        {   512,       2,      10, true  },
        {  1500,       1,      20, true  },
        { 65535,       0,      10, true  }
    };

    Terra::MemoryManager::MemoryManager memory_manager(profile);
```

Each of the Descriptors contained in the Memory Profile defines the size
of each memory allocation, a minimum count that of memory blocks to allocate,
the maximum number of blocks that should ever be kept available, and a flag
to indicate whether excess allocations requests may be made from the heap.

The number of and size of memory blocks as prescribed by the Descriptor
is something the developer must determine as suitable for the application.
In a system where a 1500 octet UDP packets is allocated and freed repeatedly,
it may be that one doesn't need more than one for the life of the program.
However, if one has a more complex system where multiple packets are received
in parallel, perhaps operated on in a pipeline (e.g., decrypting, decoding),
then one may need several memory blocks.

There are statistics collected that will reveal the total number of allocations,
the maximum number of outstanding blocks (i.e., the maximum number of memory
blocks allocated via Allocate() and not yet freed with Free()) at any point.
Such metrics can be used to gauge the sizing of the Memory Profile.

When allocating memory by calling Allocate(), the Memory Manager will look
through the Memory Profile for a Memory Descriptor having chunk sizes sufficient
to hold the requested memory.  If a block can be provided, a pointer will be
returned to the requester.  However, if a block cannot be provided from a
given Memory Descriptor, the Memory Manager will advance to the next
Memory Descriptor to try to satisfy the request.

If "Excess Allows" is set to true, requests can always be satisfied from the
Descriptor having the smallest block side unless a heap allocation fails.
Ideally, one should set that flag to false to avoid heap allocations, as
allocating from the heap on every call defeats the point of using the Memory
Manager.

One approach to addressing the sizing issue is to set the maximum allocation
value to zero.  When the maximum is zero, that means that the Memory Manager
can allocate as many blocks of the given Descriptor size as requested, but
they are never returned to the heap.  In any system that doesn't suffer
from memory leaks, this will eventually settle so that heap allocations
cease.

## Memory Allocator

The MemoryAllocator is an object that will allocate memory using a specified
Memory Manager.  The Memory Allocator implements a C++ Allocator that is
suitable for use with various STL containers like std::vector.

An example use case for the Memory Allocator is where one is receiving numerous
packets over a network interface, such as a picture in a video stream.  As each
packet is received and stored in a container, the receiver has to allocate
memory for each one. Given that this is a repetitive operation, use of
the Memory Allocator in conjunction with the Memory Manager turns this into
an exercise of moving pointers, as opposed to constantly allocating and
freeing memory.  This should result in higher performance, while reducing
memory fragmentation.

The following code snippet shows how one might use the Memory Allocator
for the aforementioned example scenario.

```cpp
    // Define the memory profile
    Terra::MemoryManager::MemoryProfile profile =
    {
        // Size, Minimum, Maximum, Excess Allowed
        {    64,       5,     100, true  },
        {   256,       2,     100, true  },
        {   512,       2,     100, true  },
        {  2048,       1,     100, true  },
        { 65536,       0,       0, true  }
    };

    // Create a Memory Manager for the given profile
    Terra::MemoryManager::MemoryManagerPointer memory_manager =
        std::make_shared<Terra::MemoryManager::MemoryManager>(profile);

    // Define a DataBuffer and vector to hold them
    using DataBuffer =
        std::vector<std::uint8_t,
                    Terra::MemoryManager::MemoryAllocator<std::uint8_t>>;
    std::vector<DataBuffer,
                Terra::MemoryManager::MemoryAllocator<DataBuffer>>
        vector(memory_manager);

    // Create 10 DataBuffers and store them in the vector
    for (std::size_t i = 0; i < 10; i++)
    {
        // Create a data buffer
        DataBuffer buffer(1500, memory_manager);

        // Store a data buffer of size 1500 in the vector
        vector.push_back(std::move(buffer));
    }
```
