#ifndef MEMORY_UTILS_H
#define MEMORY_UTILS_H

#include <cstdint>

namespace Loops::Memory
{
    inline bool IsAligned(const void* pointer, size_t alignment)
    {
        return (reinterpret_cast<uintptr_t>(pointer) % alignment) == 0;
    }

    inline size_t GetAlignmentOffset(void* pointer, size_t alignment)
    {
        uintptr_t address = reinterpret_cast<uintptr_t>(pointer);
        //size_t alignmentOffset = address % alignment; 
        // or
        size_t alignmentOffset = address & (alignment - 1);
        return alignmentOffset;
    }

    // Example: Ensure pointer is 8-byte aligned before treating as size_t*
    inline void* AlignPointer(void* pointer, size_t alignment)
    {
        size_t alignmentOffset = GetAlignmentOffset(pointer, alignment);

        if (alignmentOffset != 0)
        {
            size_t padding = alignment - alignmentOffset;
            return static_cast<unsigned char*>(pointer) + padding;
        }

        return pointer;
    }

}
#endif // !MEMORY_UTILS_H
