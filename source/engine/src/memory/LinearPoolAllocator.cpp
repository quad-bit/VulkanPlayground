#include "memory/LinearPoolAllocator.h"
#include "memory/MemoryUtils.h"
#include "Assertion.h"
#include <assert.h>

Common::Memory::LinearPoolAllocator::LinearPoolAllocator(const size_t totalSize)
{
    m_buffer = new std::byte[totalSize];

    m_start = m_buffer;
    m_head = m_start;
    m_totalSize = totalSize;
    m_end = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_start) + m_totalSize);

    assert(m_totalSize > 0);
}

void* Common::Memory::LinearPoolAllocator::Allocate(const size_t size, const size_t alignment)
{
    // check for free space
    size_t remainingSpace = reinterpret_cast<uintptr_t>(m_end) - reinterpret_cast<uintptr_t>(m_head);
    size_t headAlignmentOffset = GetAlignmentOffset(m_head, alignment);
    ASSERT_MSG(size + headAlignmentOffset <= remainingSpace, "Not enough space");

    void* dataAddress = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_head) + headAlignmentOffset);
    m_head = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_head) + headAlignmentOffset + size);

    assert(reinterpret_cast<uintptr_t>(m_head) <= reinterpret_cast<uintptr_t>(m_end));
    return dataAddress;
}

void Common::Memory::LinearPoolAllocator::Reset()
{
    m_head = m_start;
}

Common::Memory::LinearPoolAllocator::~LinearPoolAllocator()
{
    if (m_buffer)
        delete[] m_buffer;
}
