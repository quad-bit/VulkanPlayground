#include "memory/StackPoolAllocator.h"
#include "memory/MemoryUtils.h"
#include "Assertion.h"
#include <algorithm>

Common::Memory::StackPoolAllocator::StackPoolAllocator(const size_t totalSize)
{
    m_buffer = new std::byte[totalSize];

    m_start = m_buffer;
    m_head = m_start;
    m_totalSize = totalSize;
    m_end = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_start) + m_totalSize);
    m_currentHeader = nullptr;
}

void* Common::Memory::StackPoolAllocator::Allocate(const size_t size, const size_t alignment)
{
    // check for header alignment from m_head
    const size_t headerPadding = GetAlignmentOffset(m_head, alignof(Header));
    void * headerAddress = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_head) + headerPadding);
    ASSERT_MSG(IsAligned(headerAddress, alignof(Header)), "Header address not aligned");

    // Check for payload alignment from header end = headerAddress + sizeof(Header)
    void * payloadAddress = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(headerAddress) + sizeof(Header));
    const size_t payloadPadding = GetAlignmentOffset(payloadAddress, alignment);
    payloadAddress = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(headerAddress) + sizeof(Header) + payloadPadding);
    ASSERT_MSG(IsAligned(payloadAddress, alignment), "Payload address not aligned");

    size_t totalRequiredSize = headerPadding + sizeof(Header) + payloadPadding + size;
    // Check for free space
    size_t remainingSpace = reinterpret_cast<uintptr_t>(m_end) - reinterpret_cast<uintptr_t>(m_head);
    ASSERT_MSG(totalRequiredSize <= remainingSpace , "Not enough space");

    auto prevHeader = m_currentHeader;
    m_currentHeader = new (headerAddress)Header{ prevHeader, headerPadding, payloadAddress };

    m_head = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_head) + totalRequiredSize);

    return payloadAddress;
}

void Common::Memory::StackPoolAllocator::Reset()
{
    m_head = m_start;
    m_currentHeader = nullptr;
}

void Common::Memory::StackPoolAllocator::DeAllocate()
{
    // Stack pop
    if (m_currentHeader == nullptr)
        return;
    // Put the head at the last payload end
    m_head = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_currentHeader) - m_currentHeader->m_headerPadding);
    m_currentHeader = m_currentHeader->m_prevHeader;
}

Common::Memory::StackPoolAllocator::~StackPoolAllocator()
{
    if(m_buffer)
        delete[] m_buffer;
}
