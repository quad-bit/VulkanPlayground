#include "memory/MemoryManager.h"
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#include "assertion.h"

VmaAllocator Loops::Memory::MemoryManager::sm_vma = VK_NULL_HANDLE;

void Loops::Memory::MemoryManager::InitVMA(const VkPhysicalDevice& physicalDevice, const VkDevice& logicalDevice, const VkInstance& instance)
{
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = logicalDevice;
    allocatorInfo.instance = instance;
    vmaCreateAllocator(&allocatorInfo, &sm_vma);
}

void Loops::Memory::MemoryManager::DeInitVMA()
{
    vmaDestroyAllocator(sm_vma);
}

VmaAllocator& Loops::Memory::MemoryManager::GetVmaAllocator()
{
    ASSERT_MSG(sm_vma != VK_NULL_HANDLE, "use before initialise");
    return sm_vma;
}