#pragma once
#include "Utils.h"
#include "SceneNode.h"
#include "Mesh.h"

namespace Common
{
    constexpr uint32_t MAX_NODES = 1000;

    class SceneManager;

    // Child info for the parent node
    class ChildInfo
    {
    private:
        const SceneNode& m_node;
        uint32_t m_maxChildren, m_childCount = 0;
        mutable std::vector<uint32_t> m_childNodeIndicies;
    public:
        ChildInfo() = delete;
        ChildInfo(const SceneNode& node, uint32_t maxChildren);
        void AddChild(uint32_t childIndex);
        const std::tuple< std::vector<uint32_t>&, uint32_t> GetChildIndicies() const;
    };

    // Mesh Info for a node
    class MeshInfo
    {
    private:
        const SceneNode& m_node;
        const Mesh& m_mesh;
        const std::vector<Vertex>& m_vertexBuffer;
        const std::vector<uint32_t>& m_indexBuffer;

        const uint32_t GetNodeIndex() const;
        //const Mesh& GetMesh() const;

    public:
        MeshInfo() = delete;
        MeshInfo(const Common::SceneNode& node, const Mesh& mesh, const std::vector<Vertex>& vertexBuffer, const std::vector<uint32_t>& indexBuffer);

        friend class SceneManager;
    };

    class SceneManager
    {
    private:
        mutable SceneNode m_nodes[MAX_NODES];

        // there might be lesser meshes as not all nodes might have mesh
        mutable Mesh m_meshes[MAX_NODES];
        mutable uint32_t m_nodeCounter = 1; // first one is the scene parent
        mutable uint32_t m_meshCounter = 0;
        SceneNode& m_sceneRoot;
        mutable std::vector<ChildInfo> m_childInfos;
        std::vector<MeshInfo> m_meshInfos;

        ChildInfo& GetOrCreateChildInfo(const SceneNode& parentNode, const SceneNode& node);
        ChildInfo& GetChildInfo(const SceneNode& node) const;

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
        SceneManager(const std::string_view& assetPath);
        ~SceneManager();

        // Separates the loading of mesh and vulkan object creation hence multi threading can be achieved
        void Initialise(const VkDevice& device, const VkPhysicalDevice& physicalDevice, const VkQueue& graphicsQueue, uint32_t queueFamilyIndex);
        void DeInitialise();

        //SceneNode& CreateNode(const std::string& name, const NodeTransform& transform) const;
        SceneNode& CreateNode(const std::string& name, const Transform& transform, const SceneNode& parent);
        const SceneNode& GetNode(uint32_t nodeIndex) const;
        const SceneNode& GetSceneRootNode() const;
        const std::tuple< std::vector<uint32_t>&, uint32_t> GetChildIndicies(const SceneNode& parent) const;

        void IterateNodes(std::function<void(const SceneNode& node)> func) const;
        void IterateNodes(std::function<void(const SceneNode& node)> func, const SceneNode& parent) const;

        Mesh& CreateMesh(const SceneNode& node, std::vector<Common::Vertex>& vertexBuffer, std::vector<uint32_t>& indexBuffer);
        const Mesh& GetMesh(const SceneNode& node) const;

        // update the matricies
        void Update(uint32_t frameIndex);

        // prepare the rendering data maybe do culling and sorting
        void Prepare(uint32_t frameIndex);

    };
}