#include "memory/MemoryManager.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "assertion.h"

//VmaAllocator Loops::Memory::MemoryManager::sm_vma = VK_NULL_HANDLE;
Loops::Memory::MemoryManager* Loops::Memory::MemoryManager::s_instancePtr = nullptr;
std::mutex Loops::Memory::MemoryManager::s_mtx;

Loops::Memory::MemoryManager* Loops::Memory::MemoryManager::GetInstance()
{
    if (s_instancePtr == nullptr)
    {
        std::lock_guard<std::mutex> lock(s_mtx);
        if (s_instancePtr == nullptr)
        {
            s_instancePtr = new Loops::Memory::MemoryManager();
        }
    }
    return s_instancePtr;
}

void Loops::Memory::MemoryManager::Init()
{

}

void Loops::Memory::MemoryManager::InitVMA(const VkPhysicalDevice& physicalDevice, const VkDevice& logicalDevice, const VkInstance& instance)
{
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = logicalDevice;
    allocatorInfo.instance = instance;
    vmaCreateAllocator(&allocatorInfo, &m_vma);
}

void Loops::Memory::MemoryManager::DeInitPrivate()
{
    for (auto& linearPool : m_linearPools)
    {
        linearPool.reset();
        linearPool = nullptr;
    }

    for (auto& stackPool : m_stackPools)
    {
        stackPool.reset();
        stackPool = nullptr;
    }
}

void Loops::Memory::MemoryManager::DeInitVMA()
{
    vmaDestroyAllocator(m_vma);
    m_vma = VK_NULL_HANDLE;

}

void Loops::Memory::MemoryManager::DeInit()
{
    ASSERT_MSG(s_instancePtr->m_vma == VK_NULL_HANDLE, "vma not deallocated");
    s_instancePtr->DeInitPrivate();
    delete s_instancePtr;
    s_instancePtr = nullptr;
}


VmaAllocator& Loops::Memory::MemoryManager::GetVmaAllocator()
{
    ASSERT_MSG(m_vma != VK_NULL_HANDLE, "use before initialise");
    return m_vma;
}

Loops::Memory::LinearPoolAllocator* Loops::Memory::MemoryManager::CreateLinearPoolAllocator(const size_t& size)
{
    auto& pool = m_linearPools.emplace_back(std::make_unique<LinearPoolAllocator>(size));
    return pool.get();
}

Loops::Memory::StackPoolAllocator* Loops::Memory::MemoryManager::CreateStackPoolAllocator(const size_t& size)
{
    auto& pool = m_stackPools.emplace_back(std::make_unique<StackPoolAllocator>(size));
    return pool.get();
}

void* Loops::Memory::MemoryManager::AllocateFromLinearPool(const size_t size, const size_t alignment)
{
    for (auto& pool : m_linearPools)
    {
        bool spaceAvailable = pool->SpaceAvailableForAllocation(size, alignment);
        if (spaceAvailable)
        {
            void* p = pool->Allocate(size, alignment);
            return p;
        }
    }

    // create new pool, allocate and add to pool list
    auto& pool = m_linearPools.emplace_back(std::make_unique<LinearPoolAllocator>(4 * 1024 * 1024));
    void* p = pool->Allocate(size, alignment);
    return p;
}

