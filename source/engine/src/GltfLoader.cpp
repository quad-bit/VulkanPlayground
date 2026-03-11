#define TINYGLTF_IMPLEMENTATION
//#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE

#include <plog/Log.h>
#include "Utils.h"
#include <glm/gtx/quaternion.hpp>
#include <filesystem>

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
    void LoadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input,
        Common::SceneManager& sceneManager, const flecs::entity& parent,
        std::vector<Common::Vertex>& vertexBuffer, std::vector<uint32_t>& indexBuffer)
    {
        float scaleFactor = 1.0f;
        Common::Transform transform{};

        glm::mat4 matrix = glm::mat4(1.0f);
        if (inputNode.translation.size() == 3)
        {
            transform.m_position = glm::vec3(glm::make_vec3(inputNode.translation.data()));
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
        auto e = sceneManager.m_world.entity(inputNode.name.c_str()).child_of(parent);
        e.emplace<Common::Transform>(transform);

        // Load node's children
        if (inputNode.children.size() > 0)
        {
            for (size_t i = 0; i < inputNode.children.size(); i++)
            {
                LoadNode(input.nodes[inputNode.children[i]], input, sceneManager, e, vertexBuffer, indexBuffer);
            }
        }

        // If the node contains mesh data, we load vertices and indices from the buffers
        // In glTF this is done via accessors and buffer views
        if (inputNode.mesh > -1)
        {
            //Common::Mesh& meshObj = sceneManager.CreateMesh(node, vertexBuffer, indexBuffer);
            Common::Mesh meshObj;
            const tinygltf::Mesh mesh = input.meshes[inputNode.mesh];
            // Iterate through all primitives of this node's mesh
            for (size_t i = 0; i < mesh.primitives.size(); i++)
            {
                const tinygltf::Primitive& glTFPrimitive = mesh.primitives[i];
                uint32_t firstIndex = static_cast<uint32_t>(indexBuffer.size());
                uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
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
                        vert.m_position = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]), 1.0f);
                        vert.m_normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
                        vert.m_uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
                        //vert.color = glm::vec3(1.0f);
                        vert.m_tangent = tangentsBuffer ? glm::make_vec4(&tangentsBuffer[v * 4]) : glm::vec4(0.0f);
                        vertexBuffer.push_back(vert);
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
                            indexBuffer.push_back(buf[index] + vertexStart);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
                    {
                        const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            indexBuffer.push_back(buf[index] + vertexStart);
                        }
                        break;
                    }
                    case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
                    {
                        const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
                        for (size_t index = 0; index < accessor.count; index++)
                        {
                            indexBuffer.push_back(buf[index] + vertexStart);
                        }
                        break;
                    }
                    default:
                        PLOGD << "Index component type " << accessor.componentType << " not supported!";
                        assert(0);
                        return;
                    }
                }

                Common::MeshView view{};
                view.m_firstIndex = firstIndex;
                view.m_indexCount = indexCount;
                //view.materialIndex = glTFPrimitive.material;
                meshObj.m_meshViews.push_back(view);
            }
            e.emplace<Common::Mesh>(meshObj);
        }
    }
}


void Common::LoadGltf(const std::string_view& assetPath, Common::SceneManager& sceneManager,
    std::vector<Common::Vertex>& vertexBuffer, std::vector<uint32_t>& indexBuffer)
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
    const std::string& filenameWithoutExt = filePath.stem().string();

    flecs::entity modelParent = sceneManager.m_world.entity(filenameWithoutExt.c_str());
    Common::Transform t{};
    modelParent.emplace<Common::Transform>(t);

    sceneManager.AddParentEntity(modelParent);
    const tinygltf::Scene& scene = glTFInput.scenes[0];
    for (size_t i = 0; i < scene.nodes.size(); i++) 
    {
        const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
        LoadNode(node, glTFInput, sceneManager, modelParent, vertexBuffer, indexBuffer);
    }

    iterate_tree(modelParent);
}
