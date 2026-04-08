#define TINYGLTF_IMPLEMENTATION
//#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE

#include <plog/Log.h>
#include "Utils.h"
#include <glm/gtx/quaternion.hpp>
#include <filesystem>
#include <limits>
#include "GltfLoader.h"

namespace
{
    void iterate_tree(flecs::entity e) {
        // Print hierarchical name of entity & the entity type
        PLOGD << e.path() << " [" << e.type().str() << "]";

        // Iterate children recursively
        e.children([&](flecs::entity child) {
            iterate_tree(child);
        });
    }


    //https://github.com/SaschaWillems/Vulkan/blob/master/examples/gltfscenerendering/gltfscenerendering.cpp
    // vertex and index buffers
    void LoadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, flecs::world& world,
        Common::SceneManager& sceneManager, const flecs::entity& parent,
        Common::VertexBuffer& vertexBuffer, Common::IndexBuffer& indexBuffer, float scaleFactor)
    {
        Common::Transform transform{};

        glm::mat4 matrix = glm::mat4(1.0f);
        if (inputNode.translation.size() == 3)
        {
            transform.m_position = glm::vec3(glm::make_vec3(inputNode.translation.data())) * scaleFactor;
            matrix = glm::translate(matrix, transform.m_position);
        }
        if (inputNode.rotation.size() == 4)
        {
            glm::quat q = glm::make_quat(inputNode.rotation.data());
            matrix *= glm::mat4(q);

            glm::vec3 eulerRad = glm::eulerAngles(q);
            // Convert radians to degrees for readability
            transform.m_eulerAngles = glm::degrees(eulerRad);
        }
        if (inputNode.scale.size() == 3)
        {
            transform.m_scale = glm::vec3(glm::make_vec3(inputNode.scale.data()));
            matrix = glm::scale(matrix, transform.m_scale);

        }
        if (inputNode.matrix.size() == 16)
        {
            matrix = glm::make_mat4x4(inputNode.matrix.data());
        }

        transform.m_modelMat = matrix;
        auto e = world.entity(inputNode.name.c_str()).child_of(parent);
        e.emplace<Common::Transform>(transform);

        // Load node's children
        if (inputNode.children.size() > 0)
        {
            for (size_t i = 0; i < inputNode.children.size(); i++)
            {
                LoadNode(input.nodes[inputNode.children[i]], input, world, sceneManager, e, vertexBuffer, indexBuffer, scaleFactor);
            }
        }

        // If the node contains mesh data, we load vertices and indices from the buffers
        // In glTF this is done via accessors and buffer views
        if (inputNode.mesh > -1)
        {
            //Common::Mesh& meshObj = sceneManager.CreateMesh(node, vertexBuffer, indexBuffer);
            Common::Mesh meshObj;
            meshObj.m_vertexBufferIndex = vertexBuffer.m_index;
            meshObj.m_indexBufferIndex = indexBuffer.m_index;

            const tinygltf::Mesh mesh = input.meshes[inputNode.mesh];
            // Iterate through all primitives of this node's mesh
            for (size_t i = 0; i < mesh.primitives.size(); i++)
            {
                glm::vec3 max{ std::numeric_limits<float>::lowest() }, min{ std::numeric_limits<float>::max() };

                const tinygltf::Primitive& glTFPrimitive = mesh.primitives[i];
                uint32_t firstIndex = static_cast<uint32_t>(indexBuffer.m_indexList.size());
                uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.m_vertexList.size());
                uint32_t indexCount = 0;
                // Vertices
                {
                    const float* positionBuffer = nullptr;
                    const float* normalsBuffer = nullptr;
                    const float* texCoordsBuffer = nullptr;
                    const float* tangentsBuffer = nullptr;
                    size_t vertexCount = 0;

                    // Get buffer data for vertex normals
                    if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end())
                    {
                        const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("POSITION")->second];
                        const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
                        positionBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                        vertexCount = accessor.count;
                    }
                    // Get buffer data for vertex normals
                    if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end())
                    {
                        const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
                        const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
                        normalsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    }
                    // Get buffer data for vertex texture coordinates
                    // glTF supports multiple sets, we only load the first one
                    if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end())
                    {
                        const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
                        const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
                        texCoordsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    }
                    // POI: This sample uses normal mapping, so we also need to load the tangents from the glTF file
                    if (glTFPrimitive.attributes.find("TANGENT") != glTFPrimitive.attributes.end())
                    {
                        const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("TANGENT")->second];
                        const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
                        tangentsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                    }

                    // Append data to model's vertex buffer
                    for (size_t v = 0; v < vertexCount; v++)
                    {
                        Common::Vertex vert{};
                        vert.m_position = glm::vec3(glm::make_vec3(&positionBuffer[v * 3]) * scaleFactor);
                        vert.m_normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
                        vert.m_uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
                        ////vert.color = glm::vec3(1.0f);
                        vert.m_tangent = tangentsBuffer ? glm::make_vec4(&tangentsBuffer[v * 4]) : glm::vec4(0.0f);
                        vertexBuffer.m_vertexList.push_back(vert);

                        min = glm::vec3(std::min(min.x, vert.m_position.x), std::min(min.y, vert.m_position.y), std::min(min.z, vert.m_position.z));
                        max = glm::vec3(std::max(max.x, vert.m_position.x), std::max(max.y, vert.m_position.y), std::max(max.z, vert.m_position.z));
                    }

                    
                }
                // Indices
                {
                    const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.indices];
                    const tinygltf::BufferView& bufferView = input.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer& buffer = input.buffers[bufferView.buffer];

                    indexCount += static_cast<uint32_t>(accessor.count);

                    // glTF supports different component types of indices
                    switch (accessor.componentType)
                    {
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
                    {
                        const uint32_t* buf = reinterpret_cast<const uint32_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            indexBuffer.m_indexList.push_back(buf[index] + vertexStart);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
                    {
                        const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            indexBuffer.m_indexList.push_back(buf[index] + vertexStart);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                    {
                        const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            indexBuffer.m_indexList.push_back(buf[index] + vertexStart);
                        }
                        break;
                    }
                    default:
                        PLOGD << "Index component type " << accessor.componentType << " not supported!";
                        assert(0);
                        return;
                    }
                }

                Common::MeshView& view = sceneManager.GetMeshView(e, meshObj);
                assert(meshObj.m_meshViewCount < MAX_MESH_VIEWS_PER_MESH);

                view.m_firstIndex = firstIndex;
                view.m_indexCount = indexCount;
                //view.materialIndex = glTFPrimitive.material;

                {
                    sceneManager.CreateBounds(min, max, &transform.m_modelMatGlobal, view.m_viewIndex, e.id());
                }
            }
            e.emplace<Common::Mesh>(meshObj);
        }
    }
}


flecs::entity Common::LoadGltf(const std::string_view& assetPath, flecs::world& world, Common::SceneManager& sceneManager,
    Common::VertexBuffer& vertexBuffer, Common::IndexBuffer& indexBuffer, float scaleFactor)
{
    tinygltf::Model glTFInput;
    tinygltf::TinyGLTF gltfContext;
    std::string error, warning;

    bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, assetPath.data());
    assert(fileLoaded == true);

    //LoadSamplers(glTFInput);
    //LoadTextures(glTFInput);
    //LoadMaterials(glTFInput);

    std::filesystem::path filePath(assetPath.data());

    //Extract filename without extension
    const std::string& filenameWithoutExt = filePath.stem().string() + "_Parent";

    flecs::entity modelParent = world.entity(filenameWithoutExt.c_str());
    Common::Transform t{};
    modelParent.emplace<Common::Transform>(t);

    const tinygltf::Scene& scene = glTFInput.scenes[0];
    for (size_t i = 0; i < scene.nodes.size(); i++) 
    {
        const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
        LoadNode(node, glTFInput, world, sceneManager, modelParent, vertexBuffer, indexBuffer, scaleFactor);
    }

    return modelParent;
}
