#define TINYGLTF_IMPLEMENTATION
//#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE

#include <plog/Log.h>
#include "Utils.h"
#include <glm/gtx/quaternion.hpp>
#include <filesystem>
#include <limits>
#include <algorithm>
#include "GltfLoader.h"
#include "Timer.h"

#include <rapidjson/prettywriter.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/filewritestream.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/filereadstream.h>

#ifdef _WIN32  
    // Windows implementation  
#include <windows.h>  
#include <shlwapi.h>  
#elif __linux__  
    // Linux implementation  
#include <unistd.h>  
#include <limits.h>  
#endif

namespace
{
#ifdef _WIN32  
    #pragma comment(lib, "shlwapi.lib")  

    std::filesystem::path GetExecutableDirectory()
    {
        char buffer[MAX_PATH];
        DWORD length = GetModuleFileNameA(NULL, buffer, MAX_PATH);

        if (length == 0 || length == MAX_PATH) {
            throw std::runtime_error("GetModuleFileNameA failed or path truncated");
        }

        PathRemoveFileSpecA(buffer);
        return std::filesystem::path(buffer);
    }

#elif __linux__  
    // Linux implementation  

    std::filesystem::path GetExecutableDirectory()
    {
        char buffer[PATH_MAX];
        ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);

        if (length == -1)
        {
            throw std::runtime_error("readlink failed");
        }

        buffer[length] = '\0';
        return std::filesystem::path(buffer).parent_path();
    }

#else  
    // Unsupported platform  
    std::filesystem::path get_executable_directory()
    {
        throw std::runtime_error("Unsupported platform");
    }
#endif 


    void iterate_tree(flecs::entity e)
    {
        // Print hierarchical name of entity & the entity type
        PLOGD << e.path() << " [" << e.type().str() << "]";

        // Iterate children recursively
        e.children([&](flecs::entity child)
        {
            iterate_tree(child);
        });
    }

    flecs::entity CreateEntity(const tinygltf::Node& inputNode, flecs::world& world,
        const flecs::entity& parent, float scaleFactor)
    {
        Loops::Transform transform{};

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
        e.emplace<Loops::Transform>(transform);

        return e;
    }

    void FillBufferData(const float** positionBuffer, const float** normalsBuffer, const float** texCoordsBuffer, const float** tangentsBuffer, size_t& vertexCount,
        const tinygltf::Primitive& glTFPrimitive, const tinygltf::Model& input)
    {

        // Get buffer data for vertex normals
        if (glTFPrimitive.attributes.find("POSITION") != glTFPrimitive.attributes.end())
        {
            const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("POSITION")->second];
            const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
            *positionBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
            vertexCount = accessor.count;
        }
        // Get buffer data for vertex normals
        if (glTFPrimitive.attributes.find("NORMAL") != glTFPrimitive.attributes.end())
        {
            const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("NORMAL")->second];
            const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
            *normalsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
        }
        // Get buffer data for vertex texture coordinates
        // glTF supports multiple sets, we only load the first one
        if (glTFPrimitive.attributes.find("TEXCOORD_0") != glTFPrimitive.attributes.end())
        {
            const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("TEXCOORD_0")->second];
            const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
            *texCoordsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
        }
        // POI: This sample uses normal mapping, so we also need to load the tangents from the glTF file
        if (glTFPrimitive.attributes.find("TANGENT") != glTFPrimitive.attributes.end())
        {
            const tinygltf::Accessor& accessor = input.accessors[glTFPrimitive.attributes.find("TANGENT")->second];
            const tinygltf::BufferView& view = input.bufferViews[accessor.bufferView];
            *tangentsBuffer = reinterpret_cast<const float*>(&(input.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
        }
    }

    void FillIndicies(std::vector<unsigned int>& indicies, uint32_t& indexCount, const uint32_t vertexStart, const tinygltf::Primitive& glTFPrimitive,
        const tinygltf::Model& input, Loops::ModelMetadata& metadata)
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
                indicies.push_back(buf[index] + vertexStart);
            }
            break;
        }
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
        {
            const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
            for (size_t index = 0; index < accessor.count; index++)
            {
                indicies.push_back(buf[index] + vertexStart);
            }
            break;
        }
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
        {
            const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
            for (size_t index = 0; index < accessor.count; index++)
            {
                indicies.push_back(buf[index] + vertexStart);
            }
            break;
        }
        default:
            PLOGD << "Index component type " << accessor.componentType << " not supported!";
            assert(0);
            return;
        }
        metadata.m_numIndicies += indexCount;
    }

    void FillIndicies(std::vector<unsigned int>& indicies, uint32_t& indexCount, const uint32_t vertexStart, const tinygltf::Primitive& glTFPrimitive,
        const tinygltf::Model& input, size_t& arrayOffset)
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
                indicies[arrayOffset + index] = buf[index] + vertexStart;
            }
            break;
        }
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
        {
            const uint16_t* buf = reinterpret_cast<const uint16_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
            for (size_t index = 0; index < accessor.count; index++)
            {
                indicies[arrayOffset + index] = buf[index] + vertexStart;
            }
            break;
        }
        case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
        {
            const uint8_t* buf = reinterpret_cast<const uint8_t*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
            for (size_t index = 0; index < accessor.count; index++)
            {
                indicies[arrayOffset + index] = buf[index] + vertexStart;
            }
            break;
        }
        default:
            PLOGD << "Index component type " << accessor.componentType << " not supported!";
            assert(0);
            return;
        }
        arrayOffset += indexCount;
    }


    //https://github.com/SaschaWillems/Vulkan/blob/master/examples/gltfscenerendering/gltfscenerendering.cpp
    void LoadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input, flecs::world& world, Loops::SceneManager& sceneManager, Loops::BoundsManager& boundsManager,
        const flecs::entity& parent, Loops::VertexBuffer& vertexBuffer, Loops::IndexBuffer& indexBuffer, /*uint32_t& numEntities,
        uint32_t& maxMeshViewsPerMesh, uint64_t& numVerticies*/ Loops::ModelMetadata& metadata, float scaleFactor)
    {
        auto e = CreateEntity(inputNode, world, parent, scaleFactor);
        //numEntities++;
        metadata.m_numEntities++;

        // Load node's children
        if (inputNode.children.size() > 0)
        {
            for (size_t i = 0; i < inputNode.children.size(); i++)
            {
                LoadNode(input.nodes[inputNode.children[i]], input, world, sceneManager, boundsManager, e, vertexBuffer, indexBuffer, /*numEntities, maxMeshViewsPerMesh, numVerticies*/ metadata, scaleFactor);
            }
        }

        // If the node contains mesh data, we load vertices and indices from the buffers
        // In glTF this is done via accessors and buffer views
        if (inputNode.mesh > -1)
        {
            //Loops::Mesh& meshObj = sceneManager.CreateMesh(node, vertexBuffer, indexBuffer);
            Loops::Mesh meshObj;
            meshObj.m_vertexBufferIndex = vertexBuffer.m_index;
            meshObj.m_indexBufferIndex = indexBuffer.m_index;

            const tinygltf::Mesh mesh = input.meshes[inputNode.mesh];
            //maxMeshViewsPerMesh = std::max(maxMeshViewsPerMesh, (uint32_t)mesh.primitives.size());
            metadata.m_maxMeshViewsPerMesh = std::max(metadata.m_maxMeshViewsPerMesh, (uint32_t)mesh.primitives.size());

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
                    FillBufferData(&positionBuffer, &normalsBuffer, &texCoordsBuffer, &tangentsBuffer, vertexCount, glTFPrimitive, input);

                    // Append data to model's vertex buffer
                    for (size_t v = 0; v < vertexCount; v++)
                    {
                        Loops::Vertex vert{};
                        vert.m_position = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]) * scaleFactor, 1.0f);
                        vert.m_normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
                        vert.m_uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
                        ////vert.color = glm::vec3(1.0f);
                        vert.m_tangent = tangentsBuffer ? glm::make_vec4(&tangentsBuffer[v * 4]) : glm::vec4(0.0f);
                        vertexBuffer.m_vertexList.push_back(vert);

                        min = glm::vec3(std::min(min.x, vert.m_position.x), std::min(min.y, vert.m_position.y), std::min(min.z, vert.m_position.z));
                        max = glm::vec3(std::max(max.x, vert.m_position.x), std::max(max.y, vert.m_position.y), std::max(max.z, vert.m_position.z));
                    }

                    metadata.m_numVerticies += vertexCount;
                }
                // Indices
                {
                    FillIndicies(indexBuffer.m_indexList, indexCount, vertexStart, glTFPrimitive, input, metadata);
                }

                Loops::MeshView& view = sceneManager.GetMeshView(e, meshObj);
                assert(meshObj.m_meshViewCount < Loops::MAX_MESH_VIEWS_PER_MESH);

                view.m_firstIndex = firstIndex;
                view.m_indexCount = indexCount;
                //view.materialIndex = glTFPrimitive.material;

                {
                    //const glm::mat4* globalMat = &e.get<Loops::Transform>().m_modelMatGlobal;
                    boundsManager.AddBound(min, max, view.m_viewIndex, e.id());
                }
            }
            e.emplace<Loops::Mesh>(meshObj);
        }
    }

    void LoadNodeCached(const tinygltf::Node& inputNode, const tinygltf::Model& input, flecs::world& world, Loops::SceneManager& sceneManager, Loops::BoundsManager& boundsManager,
        const flecs::entity& parent, Loops::VertexBuffer& vertexBuffer, Loops::IndexBuffer& indexBuffer, float scaleFactor)
    {
        auto e = CreateEntity(inputNode, world, parent, scaleFactor);

        // Load node's children
        if (inputNode.children.size() > 0)
        {
            for (size_t i = 0; i < inputNode.children.size(); i++)
            {
                LoadNodeCached(input.nodes[inputNode.children[i]], input, world, sceneManager, boundsManager, e, vertexBuffer, indexBuffer, scaleFactor);
            }
        }

        // If the node contains mesh data, we load vertices and indices from the buffers
        // In glTF this is done via accessors and buffer views
        if (inputNode.mesh > -1)
        {
            Loops::Mesh meshObj;
            meshObj.m_vertexBufferIndex = vertexBuffer.m_index;
            meshObj.m_indexBufferIndex = indexBuffer.m_index;

            const tinygltf::Mesh mesh = input.meshes[inputNode.mesh];
            //uint32_t maxMeshViewsPerMesh = metadata.m_maxMeshViewsPerMesh;
            // Iterate through all primitives of this node's mesh
            for (size_t i = 0; i < mesh.primitives.size(); i++)
            {
                glm::vec3 max{ std::numeric_limits<float>::lowest() }, min{ std::numeric_limits<float>::max() };

                const tinygltf::Primitive& glTFPrimitive = mesh.primitives[i];

                uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.m_currentSize);
                // Vertices
                {
                    const float* positionBuffer = nullptr;
                    const float* normalsBuffer = nullptr;
                    const float* texCoordsBuffer = nullptr;
                    const float* tangentsBuffer = nullptr;
                    size_t vertexCount = 0;

                    // Get buffer data for vertex normals
                    FillBufferData(&positionBuffer, &normalsBuffer, &texCoordsBuffer, &tangentsBuffer, vertexCount, glTFPrimitive, input);

                    // Append data to model's vertex buffer
                    for (size_t v = 0; v < vertexCount; v++)
                    {
                        Loops::Vertex vert{};
                        vert.m_position = glm::vec4(glm::make_vec3(&positionBuffer[v * 3]) * scaleFactor, 1.0f);
                        vert.m_normal = glm::normalize(glm::vec3(normalsBuffer ? glm::make_vec3(&normalsBuffer[v * 3]) : glm::vec3(0.0f)));
                        vert.m_uv = texCoordsBuffer ? glm::make_vec2(&texCoordsBuffer[v * 2]) : glm::vec3(0.0f);
                        ////vert.color = glm::vec3(1.0f);
                        vert.m_tangent = tangentsBuffer ? glm::make_vec4(&tangentsBuffer[v * 4]) : glm::vec4(0.0f);
                        //vertexBuffer.m_vertexList.push_back(vert);
                        vertexBuffer.m_vertexList[vertexStart + v] = vert;

                        min = glm::vec3(std::min(min.x, vert.m_position.x), std::min(min.y, vert.m_position.y), std::min(min.z, vert.m_position.z));
                        max = glm::vec3(std::max(max.x, vert.m_position.x), std::max(max.y, vert.m_position.y), std::max(max.z, vert.m_position.z));
                    }
                    vertexBuffer.m_currentSize += vertexCount;
                }

                // Indices
                uint32_t firstIndex = static_cast<uint32_t>(indexBuffer.m_currentSize);
                uint32_t indexCount = 0;
                {
                    FillIndicies(indexBuffer.m_indexList, indexCount, vertexStart, glTFPrimitive, input, indexBuffer.m_currentSize);
                }

                Loops::MeshView& view = sceneManager.GetMeshView(e, meshObj);
                assert(meshObj.m_meshViewCount < Loops::MAX_MESH_VIEWS_PER_MESH);

                view.m_firstIndex = firstIndex;
                view.m_indexCount = indexCount;
                //view.materialIndex = glTFPrimitive.material;

                {
                    //const glm::mat4* globalMat = &e.get<Loops::Transform>().m_modelMatGlobal;
                    boundsManager.AddBound(min, max, view.m_viewIndex, e.id());
                }
            }
            e.emplace<Loops::Mesh>(meshObj);
        }
    }
}


flecs::entity Loops::LoadGltf(const std::string_view& assetPath, flecs::world& world, Loops::SceneManager& sceneManager, Loops::BoundsManager& boundsManager,
    Loops::VertexBuffer& vertexBuffer, Loops::IndexBuffer& indexBuffer, uint32_t& numEntities, uint32_t& maxMeshViewsPerMesh, float scaleFactor)
{
    //Loops::Benchmark benchMark("LoadGltf");

    const std::filesystem::path filePath(assetPath.data());
    const std::string metadataFolderPath = GetExecutableDirectory().string();
    const std::string modelName = filePath.stem().string();
    const std::string metadataFilePath = metadataFolderPath + "\\" + modelName + ".json";

    ModelMetadata metadata{};
    bool metaDataExists = false;
    if (std::filesystem::exists(metadataFilePath))
    {
        // Read metadata and send it LoadNodeCached
        std::ifstream ifs(metadataFilePath);
        assert(ifs.is_open());

        rapidjson::IStreamWrapper isw(ifs);
        rapidjson::Document doc;
        doc.ParseStream(isw);

        auto GetInt = [&doc](const std::string& name) -> uint32_t
        {
            assert(doc.HasMember(name.c_str()));
            return doc[name.c_str()].GetInt();
        };

        metadata.m_numEntities = GetInt("numEntities");
        metadata.m_numVerticies = GetInt("numVerticies");
        metadata.m_numIndicies = GetInt("numIndicies");
        metadata.m_maxMeshViewsPerMesh = GetInt("maxMeshViewsPerMesh");

        metaDataExists = true;
    }
    else
        metadata.m_numEntities = 1; // main scene parent

    //Extract filename without extension + _root
    const std::string& parentName = modelName + "_Root";

    tinygltf::Model glTFInput;
    tinygltf::TinyGLTF gltfContext;
    std::string error, warning;

    {
        //Loops::Benchmark benchMark("FileLoad");

        bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, assetPath.data());
        assert(fileLoaded == true);
    }
    //LoadSamplers(glTFInput);
    //LoadTextures(glTFInput);
    //LoadMaterials(glTFInput);

    flecs::entity modelParent = world.entity(parentName.c_str());
    Loops::Transform t{};
    modelParent.emplace<Loops::Transform>(t);

    const tinygltf::Scene& scene = glTFInput.scenes[0];
    for (size_t i = 0; i < scene.nodes.size(); i++) 
    {
        const tinygltf::Node node = glTFInput.nodes[scene.nodes[i]];
        if (metaDataExists)
        {
            vertexBuffer.m_vertexList.resize(metadata.m_numVerticies);
            indexBuffer.m_indexList.resize(metadata.m_numIndicies);
            LoadNodeCached(node, glTFInput, world, sceneManager, boundsManager, modelParent, vertexBuffer, indexBuffer, scaleFactor);
        }
        else
        {
            LoadNode(node, glTFInput, world, sceneManager, boundsManager, modelParent, vertexBuffer, indexBuffer, metadata, scaleFactor);
        }
    }

    // Write gltf cache json file
    if(!metaDataExists)
    {
        //assert(metadata.has_value());
        rapidjson::Document d;
        d.SetObject();
        d.AddMember("numEntities", metadata.m_numEntities, d.GetAllocator());
        d.AddMember("numVerticies", metadata.m_numVerticies, d.GetAllocator());
        d.AddMember("numIndicies", metadata.m_numIndicies, d.GetAllocator());
        d.AddMember("maxMeshViewsPerMesh", metadata.m_maxMeshViewsPerMesh, d.GetAllocator());

        // Open file for writing
        FILE* fp = fopen(metadataFilePath.c_str(), "wb"); // for non windows use "w"
        char writeBuffer[65536];
        rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));

        // Write the JSON data
        rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);
        d.Accept(writer);
        fclose(fp);
    }

    return modelParent;
}
