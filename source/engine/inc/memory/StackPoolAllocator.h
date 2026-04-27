#ifndef STACK_POOL_ALLOCATOR_H
#define STACK_POOL_ALLOCATOR_H

#include <memory>
#include "Assertion.h"

namespace Common::Memory
{
    class StackPoolAllocator
    {
    private:

        struct Header
        {
            Header* m_prevHeader = nullptr;
            // Padding before header to align
            size_t m_headerPadding = 0;
            void* m_payloadAddress = nullptr;
        };

        void* m_start = nullptr;
        void* m_end = nullptr;
        void* m_head = nullptr;
        std::byte* m_buffer = nullptr;
        size_t m_totalSize = 0;
        Header* m_currentHeader = nullptr;

        StackPoolAllocator() {}
        StackPoolAllocator(StackPoolAllocator const&) {}
        StackPoolAllocator const& operator= (StackPoolAllocator const&) {}

        void DeAllocate();

    public:
        explicit StackPoolAllocator(const size_t totalSize);
        void* Allocate(const size_t size, const size_t alignment = alignof(std::max_align_t));

        template<typename U, typename... Args>
        U* Allocate(Args&&... args);

        void Reset();

        template<typename U>
        void DeAllocate(U* p);

        ~StackPoolAllocator();
    };

    template<typename U, typename ...Args>
    inline U* StackPoolAllocator::Allocate(Args && ...args)
    {
        U* p = reinterpret_cast<U*>(Allocate(sizeof(U), alignof(U)));
        new(p) U(std::forward<Args>(args)...);
        return p;
    }


    template<typename U>
    inline void StackPoolAllocator::DeAllocate(U* p)
    {
        // check if the address is same top payload's address
        {
            ASSERT_MSG(p == m_currentHeader->m_payloadAddress, "Only top of stack can be deallocated");
        }
        p->~U();
        DeAllocate();
    }
}
#endif // !STACK_POOL_ALLOCATOR_H
