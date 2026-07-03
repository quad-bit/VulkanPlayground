#ifndef LINEAR_POOL_ALLOCATOR_H
#define LINEAR_POOL_ALLOCATOR_H

#include <cstddef>
#include <utility>

namespace Loops::Memory
{
    class LinearPoolAllocator
    {
    private:
        void* m_start = nullptr;
        void* m_end = nullptr;
        void* m_head = nullptr;
        std::byte* m_buffer = nullptr;
        size_t m_totalSize = 0;

        LinearPoolAllocator() {}
        LinearPoolAllocator(LinearPoolAllocator const&) {}
        LinearPoolAllocator const& operator= (LinearPoolAllocator const&) {}

    public:
        explicit LinearPoolAllocator(const size_t totalSize);
        void* Allocate(const size_t size, const size_t alignment = alignof(std::max_align_t));

        template<typename U, typename... Args>
        U* Allocate(Args&&... args);

        template<typename U>
        void DeAllocate(U* p);

        void Reset();

        bool SpaceAvailableForAllocation(const size_t size, const size_t alignment);

        ~LinearPoolAllocator();
    };

    template<typename U, typename ...Args>
    inline U* LinearPoolAllocator::Allocate(Args && ...args)
    {
        U* p = reinterpret_cast<U*>(Allocate(sizeof(U), alignof(U)));
        new(p) U(std::forward<Args>(args)...);
        return p;
    }

    template<typename U>
    inline void LinearPoolAllocator::DeAllocate(U* p)
    {
        p->~U();
    }
}
#endif // !LINEAR_POOL_ALLOCATOR_H

