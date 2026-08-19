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
#include <unordered_map>

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

    m_world.progress();

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
    // get rid of clearing, get the total number of views per material
    // and use fixed size vectors

    const auto& materialList = mp_materialManager->GetSceneMaterials();

#if 0
    auto AddWithoutCulling = [this, &renderData]() -> void
        {
            std::unordered_map<uint32_t, uint32_t> entityIdToMatrixIndexMap;
            uint32_t matrixCount = 0;
            auto& boundInfoList = m_boundManager.GetPrimtiveBoundInfo();
            Drawable* currentDrawable = nullptr; // &renderData.m_drawables[renderData.m_drawableCount];
            auto AddAllEntities = [&renderData, &entityIdToMatrixIndexMap, &matrixCount,
                &currentDrawable, this](auto self, const flecs::entity& e)-> void
                {
                    if (e.has<Loops::Mesh>())
                    {
                        Mesh m = e.get<Loops::Mesh>();
                        if (m.m_meshViewCount > 0)
                        {
                            // get rid of iterating and finding
                            for (int i = 0; i < m.m_meshViewCount; i++)
                            {
                                auto& view = m.m_meshViews[i];
                                const uint32_t entityId = e.id();
                                auto it2 = entityIdToMatrixIndexMap.find(entityId);

                                // Matrix
                                uint32_t matrixIndex;
                                if (it2 == entityIdToMatrixIndexMap.end())
                                {
                                    matrixIndex = matrixCount++;
                                    Loops::ASSERT_MSG_DEBUG(matrixCount <= MAX_ENTITIES, "Count exceeded");
                                    renderData.m_modelMats[matrixIndex] = e.get<Loops::Transform>().m_modelMatGlobal;
                                    entityIdToMatrixIndexMap.insert({ entityId, matrixIndex });
                                }
                                else
                                    matrixIndex = it2->second;

                                // View
                                const uint32_t viewIndex = renderData.m_viewCount++;
                                renderData.m_meshViews[viewIndex] = view;

                                const uint32_t materialIndex = view.m_materialIndex;
                                uint32_t drawableIndex = 0;

                                // Drawable
                                if (currentDrawable == nullptr)
                                {
                                    drawableIndex = renderData.m_drawableCount++;
                                    currentDrawable = &renderData.m_drawables[drawableIndex];
                                    currentDrawable->m_numOfViews = 1;
                                    currentDrawable->m_matrixIndex = matrixIndex;
                                    currentDrawable->m_viewStartIndex = viewIndex;
                                    currentDrawable->m_materialIndex = materialIndex;
                                    currentDrawable->m_vertexBufferId = m.m_vertexBufferIndex;
                                    currentDrawable->m_indexBufferId = m.m_indexBufferIndex;
                                }
                                else
                                {
                                    if (currentDrawable->m_materialIndex == materialIndex &&
                                        currentDrawable->m_matrixIndex == matrixIndex)
                                    {
                                        currentDrawable->m_numOfViews++;
                                        // as the drawable count is pointing 1 ahead
                                        drawableIndex = renderData.m_drawableCount - 1;
                                    }
                                    else
                                    {
                                        drawableIndex = renderData.m_drawableCount++;
                                        currentDrawable = &renderData.m_drawables[drawableIndex];
                                        currentDrawable->m_numOfViews = 1;
                                        currentDrawable->m_matrixIndex = matrixIndex;
                                        currentDrawable->m_viewStartIndex = viewIndex;
                                        currentDrawable->m_materialIndex = materialIndex;
                                        currentDrawable->m_vertexBufferId = m.m_vertexBufferIndex;
                                        currentDrawable->m_indexBufferId = m.m_indexBufferIndex;
                                    }
                                }

                                // Material map
                                const auto& material = mp_materialManager->GetSceneMaterials().at(materialIndex);

                                const Loops::EFFECT_TYPE& effectType = material.m_effect;
                                const Loops::TECHNIQUE_TYPE& techniqueType = material.m_techniqueType;

                                auto it3 = renderData.m_drawablesPerMaterial.find(effectType);
                                if (it3 == renderData.m_drawablesPerMaterial.end())
                                    renderData.m_drawablesPerMaterial.insert({ effectType, { {techniqueType, {drawableIndex } } } });
                                else
                                    it3->second[techniqueType].push_back(drawableIndex);
                            }
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
    auto AddPostFrustumCulling = [this, &renderData, &materialList]()
        {
            // index of the matrix in matrix array inn renderdata
            std::unordered_map<uint32_t, uint32_t> entityIdToMatrixIndexMap;
            uint32_t matrixCount = 0;
            auto& boundInfoList = m_boundManager.GetPrimtiveBoundInfo();
            Drawable* currentDrawable = nullptr; // &renderData.m_drawables[renderData.m_drawableCount];
            auto [primitiveBoundList, numPrimitives] = m_boundManager.GetPrimitiveBounds();
            renderData.m_drawablesPerMaterial.clear();

            auto AddLeafNodePrimitivesToRenderData = [&primitiveBoundList,
                &boundInfoList, &entityIdToMatrixIndexMap, &matrixCount,
                this, &renderData, &materialList, &currentDrawable](const BVHNode* node)
                {
                    auto& leaf = std::get<BVHLeafNode>(node->m_node);
                    for (uint32_t i = 0; i < leaf.m_numBounds; i++)
                    {
                        auto it = boundInfoList.find(primitiveBoundList[leaf.m_startIndex + i].m_boundIndex);
                        ASSERT_MSG(it != boundInfoList.end(), "bound index not found");
                        auto& boundInfo = (*it).second;

                        const flecs::entity& e = m_world.entity(boundInfo.m_entityId);
                        const auto submeshId = boundInfo.m_submeshId;

                        if (e.has<Loops::Mesh>())
                        {
                            Mesh m = e.get<Loops::Mesh>();
                            if (m.m_meshViewCount > 0)
                            {
                                // get rid of iterating and finding
                                for (int i = 0; i < m.m_meshViewCount; i++)
                                {
                                    auto& view = m.m_meshViews[i];
                                    if (view.m_viewIndex == submeshId)
                                    {
                                        const uint32_t entityId = e.id();
                                        auto it2 = entityIdToMatrixIndexMap.find(entityId);

                                        // Matrix
                                        uint32_t matrixIndex;
                                        if (it2 == entityIdToMatrixIndexMap.end())
                                        {
                                            matrixIndex = matrixCount++;
                                            Loops::ASSERT_MSG_DEBUG(matrixCount <= MAX_ENTITIES, "Count exceeded");
                                            renderData.m_modelMats[matrixIndex] = e.get<Loops::Transform>().m_modelMatGlobal;
                                            entityIdToMatrixIndexMap.insert({ entityId, matrixIndex });
                                        }
                                        else
                                            matrixIndex = it2->second;

                                        // View
                                        const uint32_t viewIndex = renderData.m_viewCount++;
                                        renderData.m_meshViews[viewIndex] = view;

                                        const uint32_t materialIndex = view.m_materialIndex;
                                        uint32_t drawableIndex = 0;

                                        // Drawable
                                        if (currentDrawable == nullptr)
                                        {
                                            drawableIndex = renderData.m_drawableCount++;
                                            currentDrawable = &renderData.m_drawables[drawableIndex];
                                            currentDrawable->m_numOfViews = 1;
                                            currentDrawable->m_matrixIndex = matrixIndex;
                                            currentDrawable->m_viewStartIndex = viewIndex;
                                            currentDrawable->m_materialIndex = materialIndex;
                                            currentDrawable->m_vertexBufferId = m.m_vertexBufferIndex;
                                            currentDrawable->m_indexBufferId = m.m_indexBufferIndex;
                                        }
                                        else
                                        {
                                            if (currentDrawable->m_materialIndex == materialIndex &&
                                                currentDrawable->m_matrixIndex == matrixIndex)
                                            {
                                                currentDrawable->m_numOfViews++;
                                                // as the drawable count is pointing 1 ahead
                                                drawableIndex = renderData.m_drawableCount - 1;
                                            }
                                            else
                                            {
                                                drawableIndex = renderData.m_drawableCount++;
                                                currentDrawable = &renderData.m_drawables[drawableIndex];
                                                currentDrawable->m_numOfViews = 1;
                                                currentDrawable->m_matrixIndex = matrixIndex;
                                                currentDrawable->m_viewStartIndex = viewIndex;
                                                currentDrawable->m_materialIndex = materialIndex;
                                                currentDrawable->m_vertexBufferId = m.m_vertexBufferIndex;
                                                currentDrawable->m_indexBufferId = m.m_indexBufferIndex;
                                            }
                                        }

                                        // Material map
                                        const auto& material = mp_materialManager->GetSceneMaterials().at(materialIndex);

                                        const Loops::EFFECT_TYPE& effectType = material.m_effect;
                                        const Loops::TECHNIQUE_TYPE& techniqueType = material.m_techniqueType;

                                        auto it3 = renderData.m_drawablesPerMaterial.find(effectType);
                                        if (it3 == renderData.m_drawablesPerMaterial.end())
                                            renderData.m_drawablesPerMaterial.insert({ effectType, { {techniqueType, {drawableIndex } } } });
                                        else
                                            it3->second[techniqueType].push_back(drawableIndex);
                                        break;
                                    }
                                }
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

    // set 0 binding 0 camera
    {
        CameraData uniform{ renderData.m_cameraData.m_viewMat, renderData.m_cameraData.m_projectionMat, renderData.m_cameraData.m_cameraPos };
        ASSERT_MSG(m_cameraUniformMemoryPointer != nullptr, "not yet mapped");
        memcpy(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_cameraUniformMemoryPointer) + m_cameraUniformDataSizePerFrame * currentFrameInFlight), &uniform, sizeof(CameraData));
    }

    // set 1 binding 0 transform array
    {
        ASSERT_MSG(m_transformUniformMemoryPointer != nullptr, "not yet mapped");
        memcpy(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_transformUniformMemoryPointer) + m_transformUniformDataSizePerFrame * currentFrameInFlight), renderData.m_modelMats, sizeof(glm::mat4) * renderData.m_drawableCount);
    }

}

Loops::MeshView& Loops::SceneManager::GetMeshView(flecs::entity& entity, Loops::Mesh& mesh)
{
    auto& view = mesh.m_meshViews[mesh.m_meshViewCount++]; 
    view.m_viewIndex = meshViewCount++;
    assert(meshViewCount <= MAX_ENTITIES * MAX_MESH_VIEWS_PER_MESH);
    return view;
}

void Loops::SceneManager::CreateGlobalResources()
{
    {
        // Transform array set 1
        {
            VkDescriptorSetLayoutBinding bindings[1]
            {
                {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT, nullptr}
            };

            VkDescriptorSetLayoutCreateInfo createInfo{};
            createInfo.bindingCount = 1;
            createInfo.pBindings = &bindings[0];
            createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            Loops::VkUtils::ErrorCheck(vkCreateDescriptorSetLayout(m_device, &createInfo, nullptr, &m_transformSetLayout));
        }

        VkDescriptorPoolSize pool_sizes[2] =
        {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 * m_maxFrameInFlights},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * m_maxFrameInFlights}
        };

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.flags = 0;
        poolInfo.maxSets = 4 * m_maxFrameInFlights;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = pool_sizes;
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        Loops::VkUtils::ErrorCheck(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_globalDescriptorPool));
    }

    // camera buffer and backing memory
    {
        const uint16_t numUniforms = m_maxFrameInFlights;

        m_cameraUniformDataSizePerFrame = VkUtils::GetMemoryAlignedDataSizeForBuffer(m_physicalDevice, sizeof(CameraData));
        VkUtils::CreateBufferVma(m_cameraUniformDataSizePerFrame * numUniforms, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
            Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_cameraBuffer.m_vkBuffer, m_cameraBuffer.m_vmaAllocation);
        vmaMapMemory(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_cameraBuffer.m_vmaAllocation, &m_cameraUniformMemoryPointer);
        ASSERT_MSG(m_cameraUniformMemoryPointer != nullptr, "not mapped");
    }

    // Transform set 1
    {
        const uint16_t numUniforms = m_maxFrameInFlights;

        const size_t dataSizePerFrame = VkUtils::GetMemoryAlignedDataSizeForBuffer(m_physicalDevice, sizeof(glm::mat4) * MAX_ENTITIES);
        m_transformUniformDataSizePerFrame = dataSizePerFrame;

        VkUtils::CreateBufferVma(dataSizePerFrame * numUniforms, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
            Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_transformBuffer.m_vkBuffer, m_transformBuffer.m_vmaAllocation);

        vmaMapMemory(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_transformBuffer.m_vmaAllocation, &m_transformUniformMemoryPointer);
        ASSERT_MSG(m_transformUniformMemoryPointer != nullptr, "not mapped");

        {
            m_transformSets.resize(numUniforms);

            for (uint16_t i = 0; i < numUniforms; i++)
            {
                VkDescriptorSetAllocateInfo setAllocInfo{};
                setAllocInfo.descriptorPool = m_globalDescriptorPool;
                setAllocInfo.descriptorSetCount = 1;
                setAllocInfo.pSetLayouts = &m_transformSetLayout;
                setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;

                Loops::VkUtils::ErrorCheck(vkAllocateDescriptorSets(m_device, &setAllocInfo, &m_transformSets[i]));

                const VkDescriptorBufferInfo bufferInfo{ m_transformBuffer.m_vkBuffer, i * dataSizePerFrame, dataSizePerFrame };
                const VkWriteDescriptorSet writes
                {
                    VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_transformSets[i], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bufferInfo, nullptr
                };
                vkUpdateDescriptorSets(m_device, 1, &writes, 0, nullptr);
            }
        }
    }

    // TODO: Create the global MaterialStruct and then create the material set
    // Material set 3
    {
    //    const uint16_t numUniforms = 1;// m_info.m_maxFrameInFlights;

    //    m_materialUniformDataSizePerFrame = VkUtils::GetMemoryAlignedDataSizeForBuffer(m_physicalDevice, sizeof(PhongMaterialUniform) * MaterialManager::MAX_MATERIALS);
    //    VkUtils::CreateBufferVma(m_materialUniformDataSizePerFrame * numUniforms, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU,
    //        Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_materialBuffer.m_vkBuffer, m_materialBuffer.m_vmaAllocation);
    //    vmaMapMemory(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_materialBuffer.m_vmaAllocation, &m_materialUniformMemoryPointer);
    //    ASSERT_MSG(m_materialUniformMemoryPointer != nullptr, "not mapped");

    //    {
    //        const auto& materialMap = mp_materialManager->GetSceneMaterials();
    //        m_materialArray.resize(MaterialManager::MAX_MATERIALS);
    //        for (const auto& [index, material] : materialMap)
    //        {
    //            if (material.m_effect == EFFECT_TYPE::OPAQUE_EFT && (material.m_techniqueType == TECHNIQUE_TYPE::PBR || material.m_techniqueType == TECHNIQUE_TYPE::PBR_DOUBLE_SIDED))
    //            {
    //                m_materialArray[index].m_color = material.m_materialData->m_baseColorFactor;
    //                m_materialArray[index].m_diffuseMapIndex = material.m_materialData->m_baseColorTextureIndex;
    //                const PbrMaterial* pbr = static_cast<const PbrMaterial*>(material.m_materialData);
    //                m_materialArray[index].m_normalMapIndex = pbr->m_normalTextureIndex;
    //            }
    //            /*else
    //                ASSERT_MSG_DEBUG(0, "case not handled");*/
    //        }

    //        ASSERT_MSG(m_materialUniformMemoryPointer != nullptr, "not yet mapped");
    //        memcpy(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(m_materialUniformMemoryPointer)), m_materialArray.data(), sizeof(PhongMaterialUniform) * MaterialManager::MAX_MATERIALS);
    //    }

    //    {
    //        m_materialSet.resize(numUniforms);

    //        for (uint16_t i = 0; i < numUniforms; i++)
    //        {
    //            VkDescriptorSetAllocateInfo setAllocInfo{};
    //            setAllocInfo.descriptorPool = m_globalDescriptorPool;
    //            setAllocInfo.descriptorSetCount = 1;
    //            setAllocInfo.pSetLayouts = &m_materialSetLayout;
    //            setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    //            Loops::VkUtils::ErrorCheck(vkAllocateDescriptorSets(m_info.m_device, &setAllocInfo, &m_materialSet[i]));

    //            VkDescriptorBufferInfo bufferInfo{ m_materialBuffer.m_vkBuffer, i * m_materialUniformDataSizePerFrame, m_materialUniformDataSizePerFrame };
    //            const VkWriteDescriptorSet writes
    //            {
    //                VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, m_materialSet[i], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &bufferInfo, nullptr
    //            };
    //            vkUpdateDescriptorSets(m_info.m_device, 1, &writes, 0, nullptr);
    //        }
    //    }
    }
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
        m_world.component<Material>();
        m_world.component<Light>();

        m_parentEntities.reserve(cm_maxEntities);
    }

    mp_materialManager = pMaterialManager;

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

    CreateGlobalResources();
}

void Loops::SceneManager::DeInitialise()
{
    for (uint32_t i = 0; i < m_vertexBufferWrapperCount; i++)
    {
        vmaDestroyBuffer(Loops::Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_vertexBufferWrappers[i].m_vkVertexBuffer, m_vertexBufferWrappers[i].m_vmaAllocation);
        vmaDestroyBuffer(Loops::Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_indexBufferWrappers[i].m_vkIndexBuffer, m_indexBufferWrappers[i].m_vmaAllocation);
    }

    vkDestroyDescriptorPool(m_device, m_globalDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_transformSetLayout, nullptr);

    vmaUnmapMemory(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_transformBuffer.m_vmaAllocation);
    vmaDestroyBuffer(Loops::Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_transformBuffer.m_vkBuffer, m_transformBuffer.m_vmaAllocation);

    vmaUnmapMemory(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_cameraBuffer.m_vmaAllocation);
    vmaDestroyBuffer(Loops::Memory::MemoryManager::GetInstance()->GetVmaAllocator(), m_cameraBuffer.m_vkBuffer, m_cameraBuffer.m_vmaAllocation);
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

const VkDescriptorSetLayout& Loops::SceneManager::GetTransformDescriptorSetLayout() const
{
    return m_transformSetLayout;
}

const VkDescriptorSet& Loops::SceneManager::GetTransformDescriptorSet(uint32_t frameIndex) const
{
    return m_transformSets[frameIndex];
}

const Loops::VulkanBuffer& Loops::SceneManager::GetCameraBuffer() const
{
    return m_cameraBuffer;
}

const size_t Loops::SceneManager::GetCameraDataSizePerFrame() const
{
    return m_cameraUniformDataSizePerFrame;
}

Loops::CameraData Loops::SceneManager::GetSceneViewCameraData() const
{
    const auto& position = m_sceneViewCamera.get<Loops::Transform>().m_position;
    glm::vec4 cameraPos{ position.x, position.y, position.z, 1.0f};
    const Loops::Camera& cam = m_sceneViewCamera.get<Loops::Camera>();
    CameraData data{ cam.GetViewMatrix(), cam.GetProjectionMat(), cameraPos};
    return data;
}

const glm::mat4& Loops::SceneManager::GetMainCameraTransform() const
{
    return m_sceneViewCamera.get<Loops::Transform>().m_modelMatGlobal;
}

