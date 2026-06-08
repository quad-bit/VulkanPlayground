#ifndef VULKAN_WRAPPERS_H
#define VULKAN_WRAPPERS_H

#include <vk_mem_alloc.h>
//#include "Utils.h"
//#include "Components.h"
#include "Defines.h"
#include <vector>

namespace Loops
{
    struct VertexBuffer
    {
        VkBuffer m_vkVertexBuffer = VK_NULL_HANDLE;
        //VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
        VmaAllocation m_vmaAllocation = VK_NULL_HANDLE;
        std::vector<Loops::Vertex> m_vertexList;
        // Used to get the position of head in case its resized with size initially
        size_t m_currentSize = 0;
        // id used for indexing in to an array of VertexBuffer
        uint32_t m_index = 0;
    };

    struct IndexBuffer
    {
        VkBuffer m_vkIndexBuffer = VK_NULL_HANDLE;
        //VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;
        VmaAllocation m_vmaAllocation = VK_NULL_HANDLE;
        std::vector<uint32_t> m_indexList;
        // Used to get the position of head in case its resized with size initially
        size_t m_currentSize = 0;
        // id used for indexing in to an array of IndexBuffer
        uint32_t m_index = 0;
    };

    struct VulkanBuffer
    {
        VkBuffer m_vkBuffer = VK_NULL_HANDLE;
        VmaAllocation m_vmaAllocation = VK_NULL_HANDLE;
    };

    struct VulkanImage
    {
        VkImage m_vkImage = VK_NULL_HANDLE;
        VkImageView m_vkImageView = VK_NULL_HANDLE;
        VmaAllocation m_vmaAllocation = VK_NULL_HANDLE;
    };
}

#endif //VULKAN_WRAPPERS_H