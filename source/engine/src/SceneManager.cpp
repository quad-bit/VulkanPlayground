#pragma once
#include "Utils.h"
#include "SceneManager.h"
#include "GltfLoader.h"
#include "Components.h"
#include "memory/MemoryManager.h"
#include "MaterialManager.h"
#include <plog/Log.h>
#include <glm/gtx/euler_angles.hpp>
#include <stack>

uint32_t meshViewCount = 0;

void Loops::SceneManager::Update(uint32_t currentFrameInFlight)
{
    std::stack<glm::mat4> matrixStack;
    matrixStack.push(glm::mat4(1.0));

    auto UpdateGlobalMatrix = [&matrixStack](auto self, const flecs::entity& e) -> void
    {
        Transform& t = e.get_mut<Loops::Transform>();
        {
            auto translationMat = glm::translate(t.m_position);
            auto scaleMat = glm::scale(t.m_scale);

            glm::mat4 rotXMat = glm::rotate(t.m_eulerAngles.x, glm::vec3(1, 0, 0));
            glm::mat4 rotYMat = glm::rotate(t.m_eulerAngles.y, glm::vec3(0, 1, 0));
            glm::mat4 rotZMat = glm::rotate(t.m_eulerAngles.z, glm::vec3(0, 0, 1));

            auto rotationMat = rotZMat * rotYMat * rotXMat;

            t.m_modelMat = translationMat * rotationMat * scaleMat;
        }

        t.m_modelMatGlobal = matrixStack.top() * t.m_modelMat;
        matrixStack.push(t.m_modelMatGlobal);

        e.children([&](const flecs::entity& child)
        {
            self(self, child);
        });

        matrixStack.pop();
    };

    for (auto& parent : m_parentEntities)
    {
        UpdateGlobalMatrix(UpdateGlobalMatrix, parent);
    }

    // camera
    {
        Loops::Transform& camTransform = m_cameraEntity.get_mut<Loops::Transform>();
        auto translationMat = glm::translate(camTransform.m_position);
        //auto scaleMat = glm::scale(camTransform.m_scale);

        glm::mat4 rotationMat(1.0);
        rotationMat = glm::rotate(rotationMat, camTransform.m_eulerAngles.x, glm::vec3(1, 0, 0));
        rotationMat = glm::rotate(rotationMat, camTransform.m_eulerAngles.y, glm::vec3(0, 1, 0));
        rotationMat = glm::rotate(rotationMat, camTransform.m_eulerAngles.z, glm::vec3(0, 0, 1));

        camTransform.m_modelMat = translationMat * rotationMat;
        camTransform.m_modelMatGlobal = camTransform.m_modelMat;

        Loops::Camera& cam = m_cameraEntity.get_mut<Loops::Camera>();
        cam.UpdateCamera(camTransform);
    }

    // Scene view camera
    {
        Loops::Transform& camTransform = m_sceneViewCamera.get_mut<Loops::Transform>();
        auto translationMat = glm::translate(camTransform.m_position);

        glm::mat4 rotXMat = glm::rotate(camTransform.m_eulerAngles.x, glm::vec3(1, 0, 0));
        glm::mat4 rotYMat = glm::rotate(camTransform.m_eulerAngles.y, glm::vec3(0, 1, 0));
        glm::mat4 rotZMat = glm::rotate(camTransform.m_eulerAngles.z, glm::vec3(0, 0, 1));
        auto rotationMat = rotZMat * rotYMat * rotXMat;

        camTransform.m_modelMat = translationMat * rotationMat;
        camTransform.m_modelMatGlobal = camTransform.m_modelMat;

        Loops::Camera& cam = m_sceneViewCamera.get_mut<Loops::Camera>();
        cam.UpdateCamera(camTransform);
    }
}

void Loops::SceneManager::Prepare(uint32_t currentFrameInFlight)
{
    RenderData& renderData = m_renderDataList[currentFrameInFlight];
    renderData.m_drawableCount = 0;
    renderData.m_viewCount = 0;

#if 0
    auto AddWithoutCulling = [this, &renderData]() -> void
        {
            auto AddAllEntities = [&renderData, this](auto self, const flecs::entity& e)-> void
                {
                    if (e.has<Loops::Mesh>())
                    {
                        Mesh m = e.get<Loops::Mesh>();
                        if (m.m_meshViewCount > 0)
                        {
                            auto& drawable = renderData.m_drawables[renderData.m_drawableCount];
                            auto& mat = renderData.m_modelMats[renderData.m_drawableCount];
                            drawable.m_matIndex = renderData.m_drawableCount++;
                            assert(renderData.m_drawableCount < MAX_ENTITIES);

                            // resetting
                            drawable.m_numOfViews = 0;

                            mat = e.get<Loops::Transform>().m_modelMatGlobal;
                            drawable.m_viewStartIndex = renderData.m_viewCount;
                            drawable.m_vertexBufferId = m.m_vertexBufferIndex;
                            drawable.m_indexBufferId = m.m_indexBufferIndex;

                            for (int i = 0; i < m.m_meshViewCount; i++)
                            {
                                auto& view = m.m_meshViews[i];
                                renderData.m_meshViews[drawable.m_viewStartIndex + drawable.m_numOfViews++] = view;
                                assert(drawable.m_numOfViews <= MAX_MESH_VIEWS_PER_MESH);
                            }
                            renderData.m_viewCount += drawable.m_numOfViews;

                            // REMOVE THIS ONLY FOR DEBUGGING
                            drawable.m_name = e.name();
                        }
                    }

                    e.children([&](const flecs::entity& child)
                        {
                            self(self, child);
                        });
                };

            for (auto& parent : m_parentEntities)
            {
                AddAllEntities(AddAllEntities, parent);
            }
        };

    AddWithoutCulling();

#else
    auto AddPostFrustumCulling = [this, &renderData]()
        {
            auto& boundInfoList = m_boundManager.GetPrimtiveBoundInfo();
            auto [primitiveBoundList, numPrimitives] = m_boundManager.GetPrimitiveBounds();
            auto AddLeafNodePrimitivesToRenderData = [&primitiveBoundList, &boundInfoList, this, &renderData](const BVHNode* node)
                {
                    auto& leaf = std::get<BVHLeafNode>(node->m_node);
                    for (uint32_t i = 0; i < leaf.m_numBounds; i++)
                    {
                        auto it = boundInfoList.find(primitiveBoundList[leaf.m_startIndex + i].m_boundIndex);
                        ASSERT_MSG(it != boundInfoList.end(), "bound index not found");
                        auto& boundInfo = (*it).second;

                        const flecs::entity& e = m_world.entity(boundInfo.m_entityId);
                        auto submeshId = boundInfo.m_submeshId;

                        if (e.has<Loops::Mesh>())
                        {
                            Mesh m = e.get<Loops::Mesh>();
                            if (m.m_meshViewCount > 0)
                            {
                                auto& drawable = renderData.m_drawables[renderData.m_drawableCount];
                                auto& mat = renderData.m_modelMats[renderData.m_drawableCount];
                                drawable.m_matIndex = renderData.m_drawableCount++;
                                assert(renderData.m_drawableCount <= MAX_ENTITIES * MAX_MESH_VIEWS_PER_MESH);

                                // resetting
                                drawable.m_numOfViews = 0;

                                mat = e.get<Loops::Transform>().m_modelMatGlobal;
                                drawable.m_viewStartIndex = renderData.m_viewCount;
                                drawable.m_vertexBufferId = m.m_vertexBufferIndex;
                                drawable.m_indexBufferId = m.m_indexBufferIndex;
                                for (int i = 0; i < m.m_meshViewCount; i++)
                                {
                                    auto& view = m.m_meshViews[i];
                                    if (view.m_viewIndex == submeshId)
                                    {
                                        renderData.m_meshViews[drawable.m_viewStartIndex + drawable.m_numOfViews++] = view;
                                        assert(drawable.m_numOfViews <= MAX_MESH_VIEWS_PER_MESH);
                                        break;
                                    }
                                }
                                renderData.m_viewCount += drawable.m_numOfViews;

                                // REMOVE THIS ONLY FOR DEBUGGING
                                drawable.m_name = e.name();
                            }
                        }
                    }
                };

            const Loops::Camera& cam = m_cameraEntity.get<Loops::Camera>();
            const Loops::Transform& model = m_cameraEntity.get<Loops::Transform>();
            //m_frustumCuller.PerformFrustumCulling(m_boundManager.GetRootNode(), cam.GetViewMatrix(), cam.GetProjectionMat(), model.m_modelMatGlobal, AddLeafNodePrimitivesToRenderData);
            m_frustumCuller.PerformFrustumCulling(m_boundManager.GetRootNode(), cam, model.m_modelMatGlobal, AddLeafNodePrimitivesToRenderData);
        };

    AddPostFrustumCulling();
#endif

    ASSERT_MSG(m_cameraEntity.is_valid(), "Camera missing");

    renderData.m_cameraData.m_cameraPos = glm::vec4(m_cameraEntity.get<Loops::Transform>().m_position, 1.0f);
    const Loops::Camera& cam = m_cameraEntity.get<Loops::Camera>();
    renderData.m_cameraData.m_viewMat = cam.GetViewMatrix();
    renderData.m_cameraData.m_projectionMat = cam.GetProjectionMat();
}

Loops::MeshView& Loops::SceneManager::GetMeshView(flecs::entity& entity, Loops::Mesh& mesh)
{
    auto& view = mesh.m_meshViews[mesh.m_meshViewCount++]; 
    view.m_viewIndex = meshViewCount++;
    assert(meshViewCount <= MAX_ENTITIES * MAX_MESH_VIEWS_PER_MESH);
    return view;
}

Loops::SceneManager::SceneManager(const std::vector<ModelLoadInfo>& infos,
    BoundsManager& boundsManager, const uint32_t& maxEntities,
    Loops::MaterialManager* pMaterialManager):
    cm_maxEntities(maxEntities), m_boundManager(boundsManager)
{
    {
        //m_world.set_entity_range(0, MAX_ENTITES);
        //m_world.enable_range_check();

        m_world.component<Transform>();
        m_world.component<Mesh>();
        m_world.component<Camera>();

        m_parentEntities.reserve(cm_maxEntities);
    }

    for (auto& info : infos)
    {
        VertexBuffer& vertBufWrapper = m_vertexBufferWrappers[m_vertexBufferWrapperCount];
        vertBufWrapper.m_index = m_vertexBufferWrapperCount++;
        assert(m_vertexBufferWrapperCount < MAX_WRAPPERS);

        IndexBuffer& indBufWrapper = m_indexBufferWrappers[m_indexBufferWrapperCount];
        indBufWrapper.m_index = m_indexBufferWrapperCount++;
        assert(m_indexBufferWrapperCount < MAX_WRAPPERS);

        m_parentEntities.emplace_back(LoadGltf(info.m_path,
            m_world, *this, boundsManager,
            vertBufWrapper, indBufWrapper,
            m_maxEntities, m_maxMeshViewsPerMesh,
            pMaterialManager, info.m_scale
            ));
    }
}

Loops::SceneManager::~SceneManager()
{
}

void Loops::SceneManager::Initialise(const VkDevice& device, const VkPhysicalDevice& physicalDevice, const VkQueue& graphicsQueue, uint32_t queueFamilyIndex,
    uint32_t maxFrameInFlights, const Dimension& screenDimension, const Dimension& designDimension)
{
    m_device = device;
    m_physicalDevice = physicalDevice;
    m_graphicsQueue = graphicsQueue;
    m_queueFamilyIndex = queueFamilyIndex;
    m_maxFrameInFlights = maxFrameInFlights;
    m_renderDataList.resize(maxFrameInFlights);

    auto CreateBufferAndCopyDataVMA = [this](size_t dataSize, VkBufferUsageFlagBits usage, VkBuffer& buffer, VmaAllocation& vmaAllocation, void* data) -> void
    {
        VkBufferCreateInfo bufferInfo = {};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        //this is the total size, in bytes, of the buffer we are allocating
        bufferInfo.size = dataSize;
        //this buffer is going to be used as a Vertex Buffer
        bufferInfo.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

        VmaAllocationCreateInfo vmaallocInfo = {};
        vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        //allocate the buffer
        Loops::VkUtils::ErrorCheck(vmaCreateBuffer(Loops::Memory::MemoryManager::GetInstance()->GetVmaAllocator(), &bufferInfo, &vmaallocInfo, &buffer, &vmaAllocation, nullptr));

        // copy data into vertex and index buffer
        auto [stagingBuffer, stagingMemory] = Loops::VkUtils::CreateStagingBuffer(dataSize, m_physicalDevice, m_device);

        {
            // map and copy 
            void* pData;
            Loops::VkUtils::ErrorCheck(vkMapMemory(m_device, stagingMemory, 0, dataSize, 0, &pData));
            memcpy(pData, data, dataSize);
            vkUnmapMemory(m_device, stagingMemory);
        }

        Loops::VkUtils::CopyFromStagingBuffer(stagingBuffer, buffer, dataSize, m_device, m_graphicsQueue, m_queueFamilyIndex);

        Loops::VkUtils::DestroyBuffer(m_device, stagingBuffer);
        Loops::VkUtils::FreeMemory(m_device, stagingMemory);
    };

    assert(m_vertexBufferWrapperCount == m_indexBufferWrapperCount);
    for (uint32_t i = 0; i < m_vertexBufferWrapperCount; i++)
    {
        const size_t verticiesDataSize = sizeof(Vertex) * m_vertexBufferWrappers[i].m_vertexList.size();
        CreateBufferAndCopyDataVMA(verticiesDataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_vertexBufferWrappers[i].m_vkVertexBuffer,
            m_vertexBufferWrappers[i].m_vmaAllocation, m_vertexBufferWrappers[i].m_vertexList.data());

        std::vector<Vertex>().swap(m_vertexBufferWrappers[i].m_vertexList);

        const size_t indiciesDataSize = sizeof(uint32_t) * m_indexBufferWrappers[i].m_indexList.size();
        CreateBufferAndCopyDataVMA(indiciesDataSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, m_indexBufferWrappers[i].m_vkIndexBuffer,
            m_indexBufferWrappers[i].m_vmaAllocation, m_indexBufferWrappers[i].m_indexList.data());
        std::vector<uint32_t>().swap(m_indexBufferWrappers[i].m_indexList);
    }

    {
        Loops::Transform camTransform{};
        Loops::Camera camera(camTransform.m_modelMatGlobal, designDimension.m_width / (float)designDimension.m_height, .2f, 70.0f, 25.0f);
        m_cameraEntity = m_world.entity("MainCamera");
        m_cameraEntity.emplace<Loops::Transform>(camTransform);
        m_cameraEntity.emplace<Loops::Camera>(camera);
    }

    {
        Loops::Transform camTransform{};
        camTransform.m_position = glm::vec3(0, 120, 0);
        //camTransform.m_position = glm::vec3(0, 40, 0);
        camTransform.m_eulerAngles = glm::vec3(glm::radians(90.0f), 0, 0);
        //camTransform.m_position = glm::vec3(40, 0, 0);
        //camTransform.m_eulerAngles = glm::vec3(0, glm::radians(-90.0f), 0);

        Loops::Camera camera(camTransform.m_modelMatGlobal, designDimension.m_width / (float)designDimension.m_height, .2f, 1000.0f, 60.0f);
        m_sceneViewCamera = m_world.entity("SceneViewCamera");
        m_sceneViewCamera.emplace<Loops::Transform>(camTransform);
        m_sceneViewCamera.emplace<Loops::Camera>(camera);
    }
    //Loops::Transform camTransform;
    ////camTransform.m_position = glm::vec3(-65, 20, 0);
    ////camTransform.m_eulerAngles = glm::vec3(glm::radians(15.0f), glm::radians(90.0f), 0);

    //// beautiful game camera
    //camTransform.m_position = glm::vec3(0, 30, -70);
    //camTransform.m_eulerAngles = glm::vec3(glm::radians(20.0f), glm::radians(0.0f), 0);

    ////camTransform.m_position = glm::vec3(0, 0, -5);
}

void Loops::SceneManager::DeInitialise()
{
    for (uint32_t i = 0; i < m_vertexBufferWrapperCount; i++)
    {
        vmaDestroyBuffer(Loops::Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_vertexBufferWrappers[i].m_vkVertexBuffer, m_vertexBufferWrappers[i].m_vmaAllocation);
        vmaDestroyBuffer(Loops::Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_indexBufferWrappers[i].m_vkIndexBuffer, m_indexBufferWrappers[i].m_vmaAllocation);
    }
}

void Loops::SceneManager::AddParentEntity(flecs::entity e)
{
    m_parentEntities.push_back(e);
}

const std::vector<flecs::entity>& Loops::SceneManager::GetParentList() const
{
    return m_parentEntities;
}

const Loops::RenderData& Loops::SceneManager::GetRenderData(uint32_t frameIndex) const
{
    return m_renderDataList[frameIndex];
}

const VkBuffer& Loops::SceneManager::GetVertexBuffer(uint32_t id) const
{
    assert(id < m_vertexBufferWrapperCount);
    return m_vertexBufferWrappers[id].m_vkVertexBuffer;
}

const VkBuffer& Loops::SceneManager::GetIndexBuffer(uint32_t id) const
{
    assert(id < m_indexBufferWrapperCount);
    return m_indexBufferWrappers[id].m_vkIndexBuffer;
}

Loops::CameraData Loops::SceneManager::GetSceneViewCameraData() const
{
    glm::vec3 cameraPos{ m_sceneViewCamera.get<Loops::Transform>().m_position};
    const Loops::Camera& cam = m_sceneViewCamera.get<Loops::Camera>();
    CameraData data{ cam.GetViewMatrix(), cam.GetProjectionMat(), cameraPos};
    return data;
}

const glm::mat4& Loops::SceneManager::GetMainCameraTransform() const
{
    return m_sceneViewCamera.get<Loops::Transform>().m_modelMatGlobal;
}

