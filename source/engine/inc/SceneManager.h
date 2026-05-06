#ifndef SCENE_MANAGER_H
#define SCENE_MANAGER_H


#include "Utils.h"
#include "Components.h"
#include "RenderData.h"
#include "VulkanWrappers.h"
#include "BoundsManager.h"
#include "Camera.h"

#include <flecs.h>
#include <memory>

namespace Loops
{
    //constexpr uint32_t MAX_ENTITES = 1000;
    const uint16_t MAX_WRAPPERS = 10; // one for an entire gltf

    //bool HasChildren(flecs::entity e);

    struct ModelMetadata
    {
        uint32_t m_numEntities = 0;
        uint64_t m_numVerticies = 0;
        uint64_t m_numIndicies = 0;
        uint32_t m_maxMeshViewsPerMesh = 0;
    };

    struct ModelLoadInfo
    {
        float m_scale = 1.0f;
        const char* m_path;
        //std::optional<ModelMetadata> m_metaData = std::nullopt;
    };

    class SceneManager
    {
    private:

        //=================   FLECS ================================
        std::vector<flecs::entity> m_parentEntities; // the ones who don't have a parent
        //=================   FLECS ================================

        //TODO: temporary
        //std::vector<Loops::Vertex> m_vertexList;
        //std::vector<uint32_t> m_indexList;
        Loops::VertexBuffer m_vertexBufferWrappers[MAX_WRAPPERS];
        Loops::IndexBuffer m_indexBufferWrappers[MAX_WRAPPERS];
        uint32_t m_vertexBufferWrapperCount = 0, m_indexBufferWrapperCount = 0;

        // Vulkan specific
        VkDevice m_device = VK_NULL_HANDLE;
        VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
        VkQueue m_graphicsQueue = VK_NULL_HANDLE;
        uint32_t m_queueFamilyIndex = 0;

        //VkBuffer m_vertexBuffer, m_indexBuffer;
        //VkDeviceMemory m_vertexBufferMemory, m_indexBufferMemory;

        const uint32_t cm_maxEntities;
        uint32_t m_maxEntities = 0;
        uint32_t m_maxMeshViewsPerMesh = 0;

        uint32_t m_maxFrameInFlights = 0;

        std::vector<Loops::RenderData> m_renderDataList;

        //Loops::BoundsManager m_boundManager;
        std::unique_ptr<Camera> m_mainCamera;

    public:

        flecs::world m_world;

        SceneManager(const std::string_view& assetPath, const uint32_t& maxEntities);
        SceneManager(const std::vector<ModelLoadInfo>& infos, BoundsManager& boundsManager, const uint32_t& maxEntities);

        ~SceneManager();

        // Separates the loading of mesh and vulkan object creation hence multi threading can be achieved
        void Initialise(const VkDevice& device, const VkPhysicalDevice& physicalDevice, const VkQueue& graphicsQueue,
            uint32_t queueFamilyIndex, uint32_t maxFrameInFlights, const Dimension& screenDimension, const Dimension& designDimension);
        void DeInitialise();

        MeshView& GetMeshView(flecs::entity& entity, Mesh& meshObj);

        void AddParentEntity(flecs::entity e);
        const std::vector<flecs::entity>& GetParentList() const;

        // update the matricies
        void Update(uint32_t currentFrameInFlight);

        // prepare the rendering data maybe do culling and sorting
        void Prepare(uint32_t currentFrameInFlight);

        //void CreateBounds(const glm::vec3& min, const glm::vec3& max, const glm::mat4* globalMat, uint32_t submeshId, uint32_t entityId);

        const RenderData& GetRenderData(uint32_t frameIndex) const;

        const VkBuffer& GetVertexBuffer(uint32_t id) const;
        const VkBuffer& GetIndexBuffer(uint32_t id) const;

        std::unique_ptr<Camera>& GetMainCamera();
    };
}

#endif // !SCENE_MANAGER_H
