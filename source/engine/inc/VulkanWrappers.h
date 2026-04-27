#ifndef VULKAN_WRAPPERS_H
#define VULKAN_WRAPPERS_H

#include "Utils.h"
#include "Components.h"

namespace Common
{
    struct VertexBuffer
    {
        VkBuffer m_vkVertexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory m_vertexBufferMemory = VK_NULL_HANDLE;
        std::vector<Common::Vertex> m_vertexList;
        // Used to get the position of head in case its resized with size initially
        size_t m_currentSize = 0;
        // id used for indexing in to an array
        uint32_t m_index = 0;
    };

    struct IndexBuffer
    {
        VkBuffer m_vkIndexBuffer = VK_NULL_HANDLE;
        VkDeviceMemory m_indexBufferMemory = VK_NULL_HANDLE;
        std::vector<uint32_t> m_indexList;
        // Used to get the position of head in case its resized with size initially
        size_t m_currentSize = 0;
        // id used for indexing in to an array
        uint32_t m_index = 0;
    };
}

#endif //VULKAN_WRAPPERS_H