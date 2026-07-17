#define TINYGLTF_IMPLEMENTATION
//#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE

#include <plog/Log.h>
#include "Utils.h"
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <filesystem>
#include <limits>
#include <algorithm>
#include "GltfLoader.h"
#include "Timer.h"
#include "Assertion.h"
#include "TextureManager.h"

#include "MaterialManager.h"
#include <basisu/transcoder/basisu_transcoder.h>

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

    bool loadImageDataFunc(tinygltf::Image* image, const int imageIndex, std::string* error, std::string* warning, int req_width, int req_height, const unsigned char* bytes, int size, void* userData)
    {
        // KTX files will be handled by our own code
        if (image->uri.find_last_of(".") != std::string::npos)
        {
            if (image->uri.substr(image->uri.find_last_of(".") + 1) == "ktx2")
            {
                return true;
            }
        }

        return tinygltf::LoadImageData(image, imageIndex, error, warning, req_width, req_height, bytes, size, userData);
    }


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

            //glm::vec3 eulerRad = glm::eulerAngles(q);
            // Convert radians to degrees for readability
            //transform.m_eulerAngles = glm::degrees(eulerRad);

            {
                // with degrees the rotation is getting messed up because of the missing negative sign
                transform.m_eulerAngles = glm::eulerAngles(q);
            }
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

    std::vector<VkSamplerCreateInfo> LoadSamplersCreateInfos(const tinygltf::Model& gltfModel)
    {
        auto GetVkWrapMode = [](int32_t wrapMode)->VkSamplerAddressMode
            {
                switch (wrapMode) {
                case -1:
                case 10497:
                    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
                case 33071:
                    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                case 33648:
                    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
                }

                Loops::ASSERT_MSG(0, "Unknown wrap mode for getVkWrapMode: ");
                return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            };

        auto GetVkFilterMode = [](int32_t filterMode) -> VkFilter
            {
                switch (filterMode) {
                case -1:
                case 9728:
                    return VK_FILTER_NEAREST;
                case 9729:
                    return VK_FILTER_LINEAR;
                case 9984:
                    return VK_FILTER_NEAREST;
                case 9985:
                    return VK_FILTER_NEAREST;
                case 9986:
                    return VK_FILTER_LINEAR;
                case 9987:
                    return VK_FILTER_LINEAR;
                }

                Loops::ASSERT_MSG(0,"Unknown filter mode for getVkFilterMode: ");
                return VK_FILTER_NEAREST;
            };


        std::vector<VkSamplerCreateInfo> samplerInfos;
        for (tinygltf::Sampler smpl : gltfModel.samplers)
        {
            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = GetVkFilterMode(smpl.magFilter);
            samplerInfo.minFilter = GetVkFilterMode(smpl.minFilter);
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.addressModeU = GetVkWrapMode(smpl.wrapS);
            samplerInfo.addressModeV = GetVkWrapMode(smpl.wrapT);
            samplerInfo.addressModeW = samplerInfo.addressModeV;
            samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
            samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
            //samplerInfo.maxLod = (float)mipLevels;
            samplerInfo.maxAnisotropy = 8.0f;
            samplerInfo.anisotropyEnable = VK_TRUE;

            samplerInfos.push_back(samplerInfo);
        }
        return samplerInfos;
    }

    std::unordered_map<uint32_t, Loops::TEXTURE_TYPE> LoadTextureTypeIndexMap(const tinygltf::Model& gltfModel)
    {
        std::unordered_map<uint32_t, Loops::TEXTURE_TYPE> indexTypeMap;

        auto GetImageInfo = [&indexTypeMap, &gltfModel](const tinygltf::Material& mat, const std::string& textureName,
            const Loops::TEXTURE_TYPE& textureType, const tinygltf::ParameterMap& map)
            {
                auto textureIndex = map.at(textureName).TextureIndex();
                auto& tex = gltfModel.textures.at(textureIndex);
                uint32_t imageIndex = tex.source;

                if (indexTypeMap.find(imageIndex) != indexTypeMap.end())
                    PLOGD << "Image at " << imageIndex << " reused in " << textureName<<" texture at "<<textureIndex ;
                else
                    indexTypeMap.insert({ imageIndex, textureType });
            };

        for (const tinygltf::Material& mat : gltfModel.materials)
        {
            if (mat.values.find("baseColorTexture") != mat.values.end())
            {
                GetImageInfo(mat, "baseColorTexture", Loops::TEXTURE_TYPE::DIFFUSE, mat.values);
                //auto textureIndex = mat.values.at("baseColorTexture").TextureIndex();
                //auto& tex = gltfModel.textures.at(textureIndex);
                //uint32_t imageIndex = tex.source;

                ////Loops::ASSERT_MSG(indexTypeMap.find(index) == indexTypeMap.end(), "duplicate index");
                //if (indexTypeMap.find(index) == indexTypeMap.end())
                //    PLOGD << "Diffuse texture at " << index << " reused";
                //indexTypeMap.insert({ index, Loops::TEXTURE_TYPE::DIFFUSE });
            }
            if (mat.values.find("metallicRoughnessTexture") != mat.values.end())
            {
                GetImageInfo(mat, "metallicRoughnessTexture", Loops::TEXTURE_TYPE::METALLIC_ROUGHNESS_MAPS, mat.values);

                //auto index = mat.values.at("metallicRoughnessTexture").TextureIndex();
                ////Loops::ASSERT_MSG(indexTypeMap.find(index) == indexTypeMap.end(), "duplicate index");
                //if (indexTypeMap.find(index) == indexTypeMap.end())
                //    PLOGD << "OMR texture at " << index << " reused";
                //indexTypeMap.insert({ index, Loops::TEXTURE_TYPE::METALLIC_ROUGHNESS_MAPS });
            }
            if (mat.additionalValues.find("normalTexture") != mat.additionalValues.end())
            {
                GetImageInfo(mat, "normalTexture", Loops::TEXTURE_TYPE::NORMAL_MAPS, mat.additionalValues);

                //auto index = mat.additionalValues.at("normalTexture").TextureIndex();
                ////Loops::ASSERT_MSG(indexTypeMap.find(index) == indexTypeMap.end(), "duplicate index");
                //if (indexTypeMap.find(index) == indexTypeMap.end())
                //    PLOGD << "Normal texture at " << index << " reused";
                //indexTypeMap.insert({ index, Loops::TEXTURE_TYPE::NORMAL_MAPS });
            }
            if (mat.additionalValues.find("emissiveTexture") != mat.additionalValues.end())
            {
                GetImageInfo(mat, "emissiveTexture", Loops::TEXTURE_TYPE::EMMISIVE_MAPS, mat.additionalValues);

                //auto index = mat.additionalValues.at("emissiveTexture").TextureIndex();
                ////Loops::ASSERT_MSG(indexTypeMap.find(index) == indexTypeMap.end(), "duplicate index");
                //if (indexTypeMap.find(index) == indexTypeMap.end())
                //    PLOGD << "Emissive texture at " << index << " reused";
                //indexTypeMap.insert({ index, Loops::TEXTURE_TYPE::EMMISIVE_MAPS });
            }
            if (mat.additionalValues.find("occlusionTexture") != mat.additionalValues.end())
            {
                GetImageInfo(mat, "occlusionTexture", Loops::TEXTURE_TYPE::AMBIENT_OCCLUSION_MAPS, mat.additionalValues);

                //auto index = mat.additionalValues.at("occlusionTexture").TextureIndex();
                //// occlussion map and ORM map might have same index
                //if(indexTypeMap.find(index) == indexTypeMap.end())
                //    indexTypeMap.insert({ index, Loops::TEXTURE_TYPE::AMBIENT_OCCLUSION_MAPS });
            }
            // Extensions
            if (mat.extensions.find("KHR_materials_pbrSpecularGlossiness") != mat.extensions.end())
            {
                auto ext = mat.extensions.find("KHR_materials_pbrSpecularGlossiness");
                if (ext->second.Has("specularGlossinessTexture"))
                {
                    Loops::ASSERT_MSG(0, "Not handled");

                    /*GetImageInfo(mat, "metallicRoughnessTexture", Loops::TEXTURE_TYPE::METALLIC_ROUGHNESS_MAPS);

                    auto index = ext->second.Get("specularGlossinessTexture").Get("index").Get<int>();;
                    Loops::ASSERT_MSG(indexTypeMap.find(index) == indexTypeMap.end(), "duplicate index");
                    indexTypeMap.insert({ index, Loops::TEXTURE_TYPE::SPECULAR_GLOSS_MAPS });*/
                }
                if (ext->second.Has("diffuseTexture"))
                {
                    Loops::ASSERT_MSG(0, "Not handled");
                    //GetImageInfo(mat, "diffuseTexture", Loops::TEXTURE_TYPE::DIFFUSE);

                    //auto index = ext->second.Get("diffuseTexture").Get("index").Get<int>();
                    //Loops::ASSERT_MSG(indexTypeMap.find(index) == indexTypeMap.end(), "duplicate index");
                    //indexTypeMap.insert({ index, Loops::TEXTURE_TYPE::DIFFUSE });
                }
            }
        }
        return indexTypeMap;
    }

    // return texture index, mipLevels
    std::pair<uint32_t, uint32_t> LoadImageDataFromFile(const tinygltf::Image& gltfimage,
        const std::string& path,
        const Loops::TEXTURE_TYPE& textureType,
        uint32_t originalTextureIndex)
    {

        uint32_t mipLevels = 0;
        uint32_t textureIndex = 0;

        // KTX2 files need to be handled explicitly
        bool isKtx2 = false;
        if (gltfimage.uri.find_last_of(".") != std::string::npos)
        {
            if (gltfimage.uri.substr(gltfimage.uri.find_last_of(".") + 1) == "ktx2")
                isKtx2 = true;
        }

        VkFormat vulkanFormat = Loops::TextureManager::GetInstance()->GetBestFormat(textureType, isKtx2);

        if (isKtx2)
        {
            // Image is KTX2 using basis universal compression. Those images need to be loaded from disk and will be transcoded to a native GPU format
            basist::ktx2_transcoder ktxTranscoder;
            const std::string filename = path + "\\" + gltfimage.uri;
            std::ifstream ifs(filename, std::ios::binary | std::ios::in | std::ios::ate);
            if (!ifs.is_open())
            {
                Loops::ASSERT_MSG(0, "Could not load the requested image file ");
            }

            uint32_t inputDataSize = static_cast<uint32_t>(ifs.tellg());
            char* inputData = new char[inputDataSize];

            ifs.seekg(0, std::ios::beg);
            ifs.read(inputData, inputDataSize);

            bool success = ktxTranscoder.init(inputData, inputDataSize);
            Loops::ASSERT_MSG(success, "Could not initialize ktx2 transcoder for image file ");

            // Select target format based on device features (use uncompressed if none supported)
            auto targetFormat = basist::transcoder_texture_format::cTFRGBA32;

            switch (vulkanFormat)
            {
            case VK_FORMAT_BC7_UNORM_BLOCK:
                targetFormat = basist::transcoder_texture_format::cTFBC7_RGBA;
                break;
            case VK_FORMAT_BC7_SRGB_BLOCK:
                targetFormat = basist::transcoder_texture_format::cTFBC7_RGBA;
                break;
            case VK_FORMAT_BC5_SNORM_BLOCK:
                targetFormat = basist::transcoder_texture_format::cTFBC5;
                break;

            default:
                Loops::ASSERT_MSG(0, "case not handled");
                break;
            }

            const bool targetFormatIsUncompressed = basist::basis_transcoder_format_is_uncompressed(targetFormat);

            std::vector<basist::ktx2_image_level_info> levelInfos(ktxTranscoder.get_levels());
            mipLevels = ktxTranscoder.get_levels();

            // Query image level information that we need later on for several calculations
            // We only support 2D images (no cube maps or layered images)
            for (uint32_t i = 0; i < mipLevels; i++)
            {
                ktxTranscoder.get_image_level_info(levelInfos[i], i, 0, 0);
            }

            uint32_t width = levelInfos[0].m_orig_width;
            uint32_t height = levelInfos[0].m_orig_height;

            // Create one staging buffer large enough to hold all uncompressed image levels
            const uint32_t bytesPerBlockOrPixel = basist::basis_get_bytes_per_block_or_pixel(targetFormat);
            uint32_t numBlocksOrPixels = 0;
            VkDeviceSize totalBufferSize = 0;
            std::vector<Loops::MipInfo> mipInfoList(mipLevels);
            for (uint32_t i = 0; i < mipLevels; i++)
            {
                // Size calculations differ for compressed/uncompressed formats
                numBlocksOrPixels = targetFormatIsUncompressed ? levelInfos[i].m_orig_width * levelInfos[i].m_orig_height : levelInfos[i].m_total_blocks;
                totalBufferSize += numBlocksOrPixels * bytesPerBlockOrPixel;

                mipInfoList[i].width = levelInfos[i].m_orig_width;
                mipInfoList[i].height = levelInfos[i].m_orig_height;
                mipInfoList[i].numBlocksOrPixels = numBlocksOrPixels;
            }

            unsigned char* buffer = new unsigned char[totalBufferSize];
            unsigned char* bufferPtr = &buffer[0];

            success = ktxTranscoder.start_transcoding();
            if (!success)
            {
                throw std::runtime_error("Could not start transcoding for image file " + filename);
            }

            // Transcode all mip levels into the staging buffer
            for (uint32_t i = 0; i < mipLevels; i++)
            {
                // Size calculations differ for compressed/uncompressed formats
                numBlocksOrPixels = targetFormatIsUncompressed ? levelInfos[i].m_orig_width * levelInfos[i].m_orig_height : levelInfos[i].m_total_blocks;
                uint32_t outputSize = numBlocksOrPixels * bytesPerBlockOrPixel;
                if (!ktxTranscoder.transcode_image_level(i, 0, 0, bufferPtr, numBlocksOrPixels, targetFormat, 0))
                {
                    Loops::ASSERT_MSG(0, "Could not transcode the requested image file ");
                }
                bufferPtr += outputSize;
            }

            textureIndex = Loops::TextureManager::GetInstance()->CreateVulkanImage(width,
                height,
                vulkanFormat,
                VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                buffer,
                totalBufferSize,
                mipLevels,
                mipInfoList,
                bytesPerBlockOrPixel);

            delete[] buffer;
            delete[] inputData;
        }
        else
        {
            // Image is a basic glTF format like png or jpg and can be loaded directly via tinyglTF
            unsigned char* buffer = nullptr;
            VkDeviceSize bufferSize = 0;
            bool deleteBuffer = false;

            if (gltfimage.component == 3)
            {
                // Most devices don't support RGB only on Vulkan so convert if necessary
                bufferSize = gltfimage.width * gltfimage.height * 4;
                buffer = new unsigned char[bufferSize];
                unsigned char* rgba = buffer;
                const unsigned char* rgb = &gltfimage.image[0];
                for (int32_t i = 0; i < gltfimage.width * gltfimage.height; ++i)
                {
                    for (int32_t j = 0; j < 3; ++j)
                        rgba[j] = rgb[j];

                    rgba += 4;
                    rgb += 3;
                }
                deleteBuffer = true;
            }
            else
            {
                buffer = const_cast<unsigned char*>(& gltfimage.image[0]);
                bufferSize = gltfimage.image.size();
            }

            // PNG supports up to 64 bits
            if (gltfimage.pixel_type == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
            {
                vulkanFormat = VK_FORMAT_R16G16B16A16_UNORM;
            }

            uint32_t width = gltfimage.width;
            uint32_t height = gltfimage.height;
            mipLevels = static_cast<uint32_t>(floor(log2(std::max(width, height))) + 1.0);

            textureIndex = Loops::TextureManager::GetInstance()->CreateVulkanImage(width,
                height,
                vulkanFormat,
                VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT,
                buffer,
                bufferSize,
                mipLevels);

            if (deleteBuffer)
            {
                delete[] buffer;
            }
        }

        return { mipLevels, textureIndex };
    }

    void LoadTextures(const std::unordered_map<uint32_t, Loops::TEXTURE_TYPE>& textureIndexMap,
        std::vector<VkSamplerCreateInfo>& sampleInfoList,
        const tinygltf::Model& gltfModel,
        const std::string& path)
    {
        uint32_t imageIndex = 0;
        std::unordered_map<uint32_t, uint32_t> indexToMiplevelMap;
        for (const tinygltf::Image& image : gltfModel.images)
        {
            Loops::ASSERT_MSG(textureIndexMap.find((uint32_t)imageIndex) != textureIndexMap.end(), "texture type not found");
            const Loops::TEXTURE_TYPE& textureType = textureIndexMap.at(imageIndex);
            auto [mipLevels, textureIndex] = LoadImageDataFromFile(image, path, textureType, (uint32_t)imageIndex);
            indexToMiplevelMap.insert({ textureIndex, mipLevels });
            imageIndex++;
        }

        for (const tinygltf::Texture& tex : gltfModel.textures)
        {
            int source = tex.source;
            // If this texture uses the KHR_texture_basisu, we need to get the source index from the extension structure
            if (tex.extensions.find("KHR_texture_basisu") != tex.extensions.end())
            {
                auto ext = tex.extensions.find("KHR_texture_basisu");
                auto& value = ext->second.Get("source");
                source = value.Get<int>();
            }

            auto it = indexToMiplevelMap.find(source);
            Loops::ASSERT_MSG_DEBUG(it != indexToMiplevelMap.end(), "source not found");

            const uint32_t mipLevels = it->second;
            VkSamplerCreateInfo& samplerInfo = sampleInfoList[tex.sampler];
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.maxLod = (float)mipLevels;
            samplerInfo.maxAnisotropy = 8.0f;
            samplerInfo.anisotropyEnable = VK_TRUE;
            const uint32_t samplerIndex = Loops::TextureManager::GetInstance()->CreateSampler(samplerInfo);

            // a sampler combined with image forms a texture
            // creating a unique sampler for every gltf texture(sampler+image)

            const uint32_t imageIndex = it->first;
            auto textureIndex = Loops::TextureManager::GetInstance()->CreateTexture(imageIndex, samplerIndex);
        }
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

    std::vector<Loops::Material> LoadMaterials(const tinygltf::Model& gltfModel, Loops::MaterialManager* pMaterialManager)
    {
        auto CreatePBRMaterial = [](const tinygltf::Material& mat, float alphaCuttoff, Loops::PbrMaterial* pbr)
            {
                pbr->m_alphaCutoff = alphaCuttoff;
                if (mat.values.find("baseColorTexture") != mat.values.end())
                {
                    pbr->m_baseColorTextureIndex = mat.values.at("baseColorTexture").TextureIndex();
                    pbr->m_baseTextureCoordinateSet = mat.values.at("baseColorTexture").TextureTexCoord();
                }
                if (mat.values.find("metallicRoughnessTexture") != mat.values.end())
                {
                    pbr->m_metallicRoughnessTextureIndex = mat.values.at("metallicRoughnessTexture").TextureIndex();
                    pbr->m_texCoordSets.m_metallicRoughnessCoords = mat.values.at("metallicRoughnessTexture").TextureTexCoord();
                }
                if (mat.values.find("roughnessFactor") != mat.values.end())
                {
                    pbr->m_roughnessFactor = static_cast<float>(mat.values.at("roughnessFactor").Factor());
                }
                if (mat.values.find("metallicFactor") != mat.values.end())
                {
                    pbr->m_metallicFactor = static_cast<float>(mat.values.at("metallicFactor").Factor());
                }
                if (mat.values.find("baseColorFactor") != mat.values.end())
                {
                    pbr->m_baseColorFactor = glm::make_vec4(mat.values.at("baseColorFactor").ColorFactor().data());
                }
                if (mat.additionalValues.find("normalTexture") != mat.additionalValues.end())
                {
                    pbr->m_normalTextureIndex = mat.additionalValues.at("normalTexture").TextureIndex();
                    pbr->m_texCoordSets.m_normalCoords = mat.additionalValues.at("normalTexture").TextureTexCoord();
                }
                if (mat.additionalValues.find("emissiveTexture") != mat.additionalValues.end())
                {
                    pbr->m_emissiveTextureIndex = mat.additionalValues.at("emissiveTexture").TextureIndex();
                    pbr->m_texCoordSets.m_emissiveCoords = mat.additionalValues.at("emissiveTexture").TextureTexCoord();
                }
                if (mat.additionalValues.find("occlusionTexture") != mat.additionalValues.end())
                {
                    pbr->m_occlusionTextureIndex = mat.additionalValues.at("occlusionTexture").TextureIndex();
                    pbr->m_texCoordSets.m_occlusionCoords = mat.additionalValues.at("occlusionTexture").TextureTexCoord();
                }
                if (mat.additionalValues.find("emissiveFactor") != mat.additionalValues.end())
                {
                    pbr->m_emissiveFactor = glm::vec4(glm::make_vec3(mat.additionalValues.at("emissiveFactor").ColorFactor().data()), 1.0);
                }

                // Extensions
                if (mat.extensions.find("KHR_materials_pbrSpecularGlossiness") != mat.extensions.end())
                {
                    auto ext = mat.extensions.find("KHR_materials_pbrSpecularGlossiness");
                    if (ext->second.Has("specularGlossinessTexture"))
                    {
                        auto& index = ext->second.Get("specularGlossinessTexture").Get("index");
                        pbr->m_extension.m_specularGlossinessTextureIndex = index.Get<int>();
                        auto& texCoordSet = ext->second.Get("specularGlossinessTexture").Get("texCoord");
                        pbr->m_texCoordSets.m_specularGlossinessCoords = texCoordSet.Get<int>();
                        pbr->m_pbrWorkflows = Loops::PbrMaterial::PbrWorkflows::SPECULAR_GLOSSINESS;
                    }
                    if (ext->second.Has("diffuseTexture"))
                    {
                        auto& index = ext->second.Get("diffuseTexture").Get("index");
                        pbr->m_extension.m_diffuseTextureIndex = index.Get<int>();
                    }
                    if (ext->second.Has("diffuseFactor"))
                    {
                        auto& factor = ext->second.Get("diffuseFactor");
                        for (uint32_t i = 0; i < factor.ArrayLen(); i++)
                        {
                            auto& val = factor.Get(i);
                            pbr->m_extension.m_diffuseFactor[i] = val.IsNumber() ? (float)val.Get<double>() : (float)val.Get<int>();
                        }
                    }
                    if (ext->second.Has("specularFactor"))
                    {
                        auto& factor = ext->second.Get("specularFactor");
                        for (uint32_t i = 0; i < factor.ArrayLen(); i++)
                        {
                            auto& val = factor.Get(i);
                            pbr->m_extension.m_specularFactor[i] = val.IsNumber() ? (float)val.Get<double>() : (float)val.Get<int>();
                        }
                    }
                }

                if (mat.extensions.find("KHR_materials_emissive_strength") != mat.extensions.end())
                {
                    auto ext = mat.extensions.find("KHR_materials_emissive_strength");
                    if (ext->second.Has("emissiveStrength"))
                    {
                        auto& value = ext->second.Get("emissiveStrength");
                        pbr->m_emissiveStrength = (float)value.Get<double>();
                    }
                }
            };

        std::vector<Loops::Material> materialList;
        for (const tinygltf::Material& mat : gltfModel.materials)
        {
            Loops::EFFECT_TYPE effectType{ Loops::EFFECT_TYPE::OPAQUE_EFT };
            Loops::TECHNIQUE_TYPE techniqueType{ Loops::TECHNIQUE_TYPE::PBR };
            float alphaCutoffValue = 1.0f;

            Loops::Material material{};
            Loops::MaterialData* matData = nullptr;

            if (mat.additionalValues.find("alphaMode") != mat.additionalValues.end())
            {
                tinygltf::Parameter param = mat.additionalValues.at("alphaMode");
                if (param.string_value == "BLEND")
                    effectType = Loops::EFFECT_TYPE::TRANSPARENT_EFT;
                if (param.string_value == "MASK")
                {
                    alphaCutoffValue = 0.5f;
                    effectType = Loops::EFFECT_TYPE::APLHA_MASK_EFT;
                }
            }
            if (mat.extensions.find("KHR_materials_unlit") != mat.extensions.end())
            {
                PLOGD << "Unlit material";
                Loops::ASSERT_MSG(0, "Unlit not handled");
            }
            else
            {
                bool isMetallicRoughness = mat.pbrMetallicRoughness.baseColorFactor.size() > 0 ||
                    mat.pbrMetallicRoughness.baseColorTexture.index >= 0 ||
                    mat.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0;

                bool isSpecularGlossiness = false;
                auto extIt = mat.extensions.find("KHR_materials_pbrSpecularGlossiness");
                if (extIt != mat.extensions.end())
                    isSpecularGlossiness = true;

                bool isPBR = false;
                if (isMetallicRoughness || isSpecularGlossiness)
                    isPBR = true;

                if (isPBR)
                {
                    techniqueType = Loops::TECHNIQUE_TYPE::PBR;
                    matData = pMaterialManager->GetPbrMaterialRef();
                    CreatePBRMaterial(mat, alphaCutoffValue, static_cast<Loops::PbrMaterial*>(matData));
                }
                else
                    Loops::ASSERT_MSG_DEBUG(0, "Type not handled");
            }
            if (mat.doubleSided)
            {
                Loops::ASSERT_MSG(0, "Double sided not handled");
            }
            material.m_effect = effectType;
            material.m_techniqueType = techniqueType;
            material.m_materialData = matData;
            materialList.push_back(material);
        }

        return materialList;
    }

    //https://github.com/SaschaWillems/Vulkan/blob/master/examples/gltfscenerendering/gltfscenerendering.cpp
    void LoadNode(const tinygltf::Node& inputNode, const tinygltf::Model& input,
        flecs::world& world, Loops::SceneManager& sceneManager,
        Loops::BoundsManager& boundsManager, const flecs::entity& parent,
        Loops::VertexBuffer& vertexBuffer, Loops::IndexBuffer& indexBuffer,
        /*uint32_t& numEntities, uint32_t& maxMeshViewsPerMesh, uint64_t& numVerticies*/
        Loops::ModelMetadata& metadata, float scaleFactor)
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
                assert(meshObj.m_meshViewCount <= Loops::MAX_MESH_VIEWS_PER_MESH);

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
                assert(meshObj.m_meshViewCount <= Loops::MAX_MESH_VIEWS_PER_MESH);

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


flecs::entity Loops::LoadGltf(const std::string_view& assetPath,
    flecs::world& world, Loops::SceneManager& sceneManager,
    Loops::BoundsManager& boundsManager, Loops::VertexBuffer& vertexBuffer,
    Loops::IndexBuffer& indexBuffer, uint32_t& numEntities,
    uint32_t& maxMeshViewsPerMesh, Loops::MaterialManager* pMaterialManager,
    float scaleFactor)
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
    gltfContext.SetImageLoader(loadImageDataFunc, nullptr);
    {
        //Loops::Benchmark benchMark("FileLoad");

        bool fileLoaded = gltfContext.LoadASCIIFromFile(&glTFInput, &error, &warning, assetPath.data());
        assert(fileLoaded == true);

        auto& extensions = glTFInput.extensionsUsed;
        for (auto& extension : extensions)
        {
            // If this model uses basis universal compressed textures, we need to transcode them
            // So we need to initialize that transcoder once
            if (extension == "KHR_texture_basisu")
            {
                PLOGD << "Model uses KHR_texture_basisu, initializing basisu transcoder\n";
                basist::basisu_transcoder_init();
            }
        }
    }
    //LoadSamplers(glTFInput);
    //LoadTextures(glTFInput);
    //LoadMaterials(glTFInput);

    auto samplerInfos = LoadSamplersCreateInfos(glTFInput);
    auto textureIndexMap = LoadTextureTypeIndexMap(glTFInput);
    LoadTextures(textureIndexMap, samplerInfos, glTFInput, filePath.parent_path().string());
    auto materialList = LoadMaterials(glTFInput, pMaterialManager);

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
        char* writeBuffer = new char[65536];
        rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(char) * 65536);

        // Write the JSON data
        rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);
        d.Accept(writer);
        fclose(fp);

        delete writeBuffer;
    }

    return modelParent;
}
