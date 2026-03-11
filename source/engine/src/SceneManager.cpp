#pragma once
#include "Utils.h"
#include "SceneManager.h"
#include "GltfLoader.h"
#include "Components.h"
#include <plog/Log.h>

void Common::SceneManager::Update(uint32_t frameIndex)
{
}

void Common::SceneManager::Prepare(uint32_t frameIndex)
{
}

Common::SceneManager::SceneManager(const std::string_view& assetPath)
{
    {
        //m_world.set_entity_range(0, MAX_ENTITES);
        //m_world.enable_range_check();

        m_world.component<Transform>();
        m_world.component<Mesh>();

        m_parentEntities.reserve(MAX_ENTITES);
    }

    LoadGltf(assetPath, *this, m_vertexList, m_indexList);
}

Common::SceneManager::~SceneManager()
{

}

void Common::SceneManager::Initialise(const VkDevice& device, const VkPhysicalDevice& physicalDevice,
    const VkQueue& graphicsQueue, uint32_t queueFamilyIndex)
{
    m_device = device;
    m_physicalDevice = physicalDevice;
    m_graphicsQueue = graphicsQueue;
    m_queueFamilyIndex = queueFamilyIndex;

    auto CreateAndCopyData = [this](size_t dataSize, VkBufferUsageFlagBits usage, VkBuffer& buffer, VkDeviceMemory& memory, void* data) -> void
    {
        CreateBufferAndMemory(m_physicalDevice, m_device, buffer, memory, dataSize,
            usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        // copy data into vertex and index buffer

        auto [stagingBuffer, stagingMemory] = CreateStagingBuffer(dataSize, m_physicalDevice, m_device);

        {
            // map and copy 
            void* pData;
            ErrorCheck(vkMapMemory(m_device, stagingMemory, 0, dataSize, 0, &pData));
            memcpy(pData, data, dataSize);
            vkUnmapMemory(m_device, stagingMemory);
        }

        CopyFromStagingBuffer(stagingBuffer, buffer, dataSize, m_device, m_graphicsQueue, m_queueFamilyIndex);

        DestroyBuffer(m_device, stagingBuffer);
        FreeMemory(m_device, stagingMemory);
    };

    const size_t verticiesDataSize = sizeof(Vertex) * m_vertexList.size();
    const size_t indiciesDataSize = sizeof(uint32_t) * m_indexList.size();

    CreateAndCopyData(verticiesDataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_vertexBuffer, m_vertexBufferMemory, m_vertexList.data());
    CreateAndCopyData(indiciesDataSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, m_indexBuffer, m_indexBufferMemory, m_indexList.data());
}

void Common::SceneManager::DeInitialise()
{
    DestroyBuffer(m_device, m_vertexBuffer);
    DestroyBuffer(m_device, m_indexBuffer);
    FreeMemory(m_device, m_vertexBufferMemory);
    FreeMemory(m_device, m_indexBufferMemory);
}

void Common::SceneManager::AddParentEntity(flecs::entity e)
{
    m_parentEntities.push_back(e);
}

const std::vector<flecs::entity>& Common::SceneManager::GetParentList() const
{
    return m_parentEntities;
}

bool Common::HasChildren(flecs::entity e)
{
    bool found = false;
    e.children([&](flecs::entity child) {
        found = true;
        return; // Stop after finding the first child
    });
    return found;
};

