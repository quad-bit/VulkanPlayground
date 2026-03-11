#pragma once
#include "Utils.h"
#include "Components.h"
#include <flecs.h>

namespace Common
{
    constexpr uint32_t MAX_ENTITES = 1000;

    bool HasChildren(flecs::entity e);

    class SceneManager;

    class SceneManager
    {
    private:

        //=================   FLECS ================================
        std::vector<flecs::entity> m_parentEntities; // the ones who don't have a parent
        //=================   FLECS ================================

        //TODO: temporary
        std::vector<Common::Vertex> m_vertexList;
        std::vector<uint32_t> m_indexList;

        // Vulkan specific
        VkDevice m_device = VK_NULL_HANDLE;
        VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
        VkQueue m_graphicsQueue = VK_NULL_HANDLE;
        uint32_t m_queueFamilyIndex = 0;

        VkBuffer m_vertexBuffer, m_indexBuffer;
        VkDeviceMemory m_vertexBufferMemory, m_indexBufferMemory;

    public:

        flecs::world m_world;

        SceneManager(const std::string_view& assetPath);
        ~SceneManager();

        // Separates the loading of mesh and vulkan object creation hence multi threading can be achieved
        void Initialise(const VkDevice& device, const VkPhysicalDevice& physicalDevice, const VkQueue& graphicsQueue, uint32_t queueFamilyIndex);
        void DeInitialise();

        void AddParentEntity(flecs::entity e);
        const std::vector<flecs::entity>& GetParentList() const;

        // update the matricies
        void Update(uint32_t frameIndex);

        // prepare the rendering data maybe do culling and sorting
        void Prepare(uint32_t frameIndex);
    };
}