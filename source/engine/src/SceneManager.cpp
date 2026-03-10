#pragma once
#include "Utils.h"
#include "SceneManager.h"
#include "GltfLoader.h"

Common::ChildInfo::ChildInfo(const SceneNode& node, uint32_t maxChildren) : m_node(node), m_maxChildren(maxChildren)
{
    m_childNodeIndicies.resize(m_maxChildren);
}

void Common::ChildInfo::AddChild(uint32_t childIndex)
{
    m_childNodeIndicies[m_childCount++] = childIndex;

    assert(m_childCount < m_maxChildren);
}

const std::tuple<std::vector<uint32_t>&, uint32_t> Common::ChildInfo::GetChildIndicies() const
{
    return { m_childNodeIndicies, m_childCount };
}

Common::ChildInfo& Common::SceneManager::GetOrCreateChildInfo(const SceneNode& parentNode, const SceneNode& node)
{
    // if child info for this node doesn't exist then create a child info
    {
        if (m_childInfos.size() <= node.m_index)
        {
            // except for scene root all nodes can have max children of 10
            ChildInfo childInfo(node, 10);
            m_childInfos.push_back(childInfo);
        }
    }
    return m_childInfos[parentNode.m_index];
}

Common::ChildInfo& Common::SceneManager::GetChildInfo(const SceneNode& node) const
{
    return m_childInfos[node.m_index];
}

Common::SceneNode& Common::SceneManager::CreateNode(const std::string& name, const Transform& transform, const SceneNode& parent)
{
    assert(name.size() < 16);

    SceneNode& node = m_nodes[m_nodeCounter];
    node.m_index = m_nodeCounter;
    std::strcpy(node.m_name.data(), name.c_str());
    node.m_transform = transform;
    node.m_parentIndex = parent.m_index;
    // filled in loader
    //node.m_hasMesh = ;
    //node.m_hasChildren = ;

    // Add child info
    {
        auto& info = GetOrCreateChildInfo(parent, node);
        info.AddChild(m_nodeCounter);
    }

    m_nodeCounter++;
    return node;
}

const Common::SceneNode& Common::SceneManager::GetNode(uint32_t nodeIndex) const
{
    return m_nodes[nodeIndex];
}

const Common::SceneNode& Common::SceneManager::GetSceneRootNode() const
{
    return m_sceneRoot;
}

const std::tuple< std::vector<uint32_t>&, uint32_t> Common::SceneManager::GetChildIndicies(const SceneNode& parent) const
{
    auto& info = GetChildInfo(parent);
    return info.GetChildIndicies();
}

void Common::SceneManager::IterateNodes(std::function<void(const SceneNode& node)> func) const
{
    for (auto& node : m_nodes)
    {
        func(node);
    }
}

void Common::SceneManager::IterateNodes(std::function<void(const SceneNode& node)> func, const SceneNode& parent) const
{
    func(parent);

    if (!parent.m_hasChildren)
        return;

    // for all children
    auto& childInfo = GetChildInfo(parent);

    auto& result = childInfo.GetChildIndicies();

    for(auto i=0; i< std::get<1>(result);i++)
    {
        auto& node = GetNode(std::get<0>(result).at(i));
        IterateNodes(func, node);
    }
}

Common::Mesh& Common::SceneManager::CreateMesh(const Common::SceneNode& node,
    std::vector<Common::Vertex>& vertexBuffer, std::vector<uint32_t>& indexBuffer)
{
    auto& mesh = m_meshes[m_meshCounter++];
    m_meshInfos.emplace_back(MeshInfo{ node, mesh, vertexBuffer, indexBuffer });
    return mesh;
}

const Common::Mesh& Common::SceneManager::GetMesh(const Common::SceneNode& node) const
{
    auto itr = std::find_if(m_meshInfos.begin(), m_meshInfos.end(), [&node](const MeshInfo& info) {
        return info.GetNodeIndex() == node.GetIndex() ;
    });

    if (itr == m_meshInfos.end())
    {
        assert(0);
    }

    return  m_meshInfos[std::distance(m_meshInfos.begin(), itr)].m_mesh;
}

void Common::SceneManager::Update(uint32_t frameIndex)
{
}

void Common::SceneManager::Prepare(uint32_t frameIndex)
{
}

Common::SceneManager::SceneManager(const std::string_view& assetPath) 
    : m_sceneRoot(m_nodes[0])
{
    m_sceneRoot = {};
    m_sceneRoot.m_hasChildren = true;
    std::strcpy(m_sceneRoot.m_name.data(), "SceneRoot");

    //TODO: UGLY.. fix this
    ChildInfo rootInfo(m_sceneRoot, 1000);
    m_childInfos.push_back(rootInfo);

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

    //// Create vulkan vertex and index buffers
    //CreateBufferAndMemory(m_physicalDevice, m_device, m_vertexBuffer, m_vertexBufferMemory, verticiesDataSize,
    //    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    //CreateBufferAndMemory(m_physicalDevice, m_device, m_indexBuffer, m_indexBufferMemory, indiciesDataSize,
    //    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    //// copy data into vertex and index buffer

    //auto [stagingBuffer, stagingMemory] = CreateStagingBuffer(verticiesDataSize, m_physicalDevice, m_device);

    //{
    //    // map and copy 
    //    void* pData;
    //    ErrorCheck(vkMapMemory(m_device, stagingMemory, 0, verticiesDataSize, 0, &pData));
    //    memcpy(pData, m_vertexList.data(), verticiesDataSize);
    //    vkUnmapMemory(m_device, stagingMemory);
    //}

    //CopyFromStagingBuffer(stagingBuffer, m_vertexBuffer, verticiesDataSize, m_device, m_graphicsQueue, m_queueFamilyIndex);

    //DestroyBuffer(m_device, stagingBuffer);
    //FreeMemory(m_device, stagingMemory);

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

Common::MeshInfo::MeshInfo(const Common::SceneNode& node, const Common::Mesh& mesh,
    const std::vector<Common::Vertex>& vertexBuffer, const std::vector<uint32_t>& indexBuffer):
    m_node(node), m_mesh(mesh), m_vertexBuffer(vertexBuffer), m_indexBuffer(indexBuffer)
{
}

const uint32_t Common::MeshInfo::GetNodeIndex() const
{
    return m_node.GetIndex();
}

