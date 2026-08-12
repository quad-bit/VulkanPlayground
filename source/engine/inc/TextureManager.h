#ifndef TEXTURE_MANAGER
#define TEXTURE_MANAGER

#include <memory>
#include <mutex>
#include <optional>
#include <vulkan/vulkan.h>
#include <unordered_map>
#include <VulkanWrappers.h>

namespace Loops
{
    constexpr uint32_t MAX_TEXTURES = 50;
    constexpr uint32_t TEXTURE_SET_VALUE = 1;
    constexpr uint32_t TEXTURE_SET_BINDING_VALUE = 0;

    enum TEXTURE_TYPE
    {
        FBO,
        DEPTH_STENCIL,
        DIFFUSE, // rgba srgb 
        // older gltfs used diffuse texture, newer ones are using baseColorTexture
        // we are sticking to baseColorTexture
        NORMAL_MAPS, // rg snorm linear
        AMBIENT_OCCLUSION_MAPS, //r linear
        EMMISIVE_MAPS, //rgb srgb
        SPECULAR_GLOSS_MAPS, // rgba in srgb, a is for roughness in linear 
        METALLIC_ROUGHNESS_MAPS, // rgb linear
        MAP_TYPES
    };

    using DescriptorImageIndex = uint32_t;

    // will be used in descriptor bindings and in shaders
    // directly maps to gltf's texture abstraction
    struct Texture
    {
        uint32_t m_vulkanImageWrapperIndex;
        uint32_t m_samplerIndex;
    };

    struct MipInfo
    {
        uint32_t width, height;
        uint32_t numBlocksOrPixels, pad;
    };

    class TextureManager
    {
    private:
        static TextureManager* s_instancePtr;
        static std::mutex s_mtx;

        const std::unordered_map<TEXTURE_TYPE, std::vector<VkFormat>> m_preferredCompressedTextureFormatMapPC
        {
            {DIFFUSE, {VK_FORMAT_BC7_SRGB_BLOCK}},
            {NORMAL_MAPS, {VK_FORMAT_BC5_SNORM_BLOCK}},
            {AMBIENT_OCCLUSION_MAPS, {VK_FORMAT_BC4_UNORM_BLOCK}},
            {EMMISIVE_MAPS, {VK_FORMAT_BC7_SRGB_BLOCK}},// 3 channel
            {SPECULAR_GLOSS_MAPS, {VK_FORMAT_BC7_SRGB_BLOCK}},
            {METALLIC_ROUGHNESS_MAPS, {VK_FORMAT_BC7_UNORM_BLOCK}},
        };

        const std::unordered_map<TEXTURE_TYPE, std::vector<VkFormat>> m_preferredUncompressedTextureFormatMapPC
        {
            {FBO, {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM}},
            {DEPTH_STENCIL, {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT}},
            {DIFFUSE, {VK_FORMAT_R8G8B8A8_UNORM}},//SRGB is making it a bit dark, figure this out
            {NORMAL_MAPS, {VK_FORMAT_R8G8B8A8_UNORM}},
            {AMBIENT_OCCLUSION_MAPS, {VK_FORMAT_R16_UNORM}},
            {EMMISIVE_MAPS, {VK_FORMAT_R8G8B8A8_SRGB}},// 3 channel
            {SPECULAR_GLOSS_MAPS, {VK_FORMAT_R8G8B8A8_SRGB}},
            {METALLIC_ROUGHNESS_MAPS, {VK_FORMAT_R8G8B8A8_UNORM}},
        };

        const std::unordered_map<TEXTURE_TYPE, VkFormatFeatureFlags> m_textureFeatureMap
        {
            {FBO, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT},
            {DEPTH_STENCIL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT},
            {DIFFUSE, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT},
            {NORMAL_MAPS, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT},
            {AMBIENT_OCCLUSION_MAPS, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT},
            {EMMISIVE_MAPS, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT},
            {SPECULAR_GLOSS_MAPS, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT},
            {METALLIC_ROUGHNESS_MAPS, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT}
        };

        std::unordered_map<TEXTURE_TYPE, VkFormat> m_availableCompressedFormats, m_availableUncompressedFormats;
        VkDevice m_device = VK_NULL_HANDLE;
        VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
        VkQueue m_queue = VK_NULL_HANDLE;
        uint32_t m_queueFamilyIndex = 0;
        uint32_t m_maxFrameInFlights = 0;

        std::unordered_map<uint32_t, VkSampler> m_samplerMap;
        uint32_t m_samplerCounter = 0;

        std::unordered_map<uint32_t, VulkanImage> m_imageList;
        uint32_t m_imageCount = 0;

        std::unordered_map<uint32_t, Texture> m_textureList;
        uint32_t m_textureCount = 0;
        // if multiple gltf files are getting loaded the local texture
        // indicies within gltf file will start from zero
        // but we are storing all the images into one single array
        // hence while creating the textures for a gltf file 
        // we need its respective index offset in the array
        std::unordered_map<uint32_t,uint32_t> m_textureOffsetMarker;

        std::unordered_map<uint32_t, VkDescriptorImageInfo> m_descriptorImageInfoList;
        uint32_t m_descriptorImageInfoCount = 0;

        VkDescriptorPool m_textureDescriptorPool = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_textureDescriptorSetLayout = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_textureDescriptorSets;

        TextureManager() {}
        void DeInitPrivate();

    public:
        static TextureManager* GetInstance();
        static void DeInit();

        void Init(const VkPhysicalDevice& physicalDevice,
            const VkDevice& device, const VkQueue& queue,
            uint32_t queuefamilyIndex, uint32_t maxFrameInFlights);
        bool IsFormatAvailable(const VkFormat& format, const VkFormatFeatureFlags& formatFeature) const;
        VkFormat GetBestFormat(const TEXTURE_TYPE& textureType, bool isCompressed) const;

        // Meant for sampled ktx2 texture
        uint32_t CreateVulkanImage(const uint32_t width, const uint32_t height,
            const VkFormat& format, const VkImageUsageFlags& usageFlags,
            const unsigned char* data, const size_t& dataSize,
            uint32_t mipLevels, const std::vector<MipInfo>& mipInfoList,
            const uint32_t& bytesPerBlockOrPixel);

        // Meant for sampled png jpeg texture
        uint32_t CreateVulkanImage(const uint32_t width, const uint32_t height,
            const VkFormat& format, const VkImageUsageFlags& usageFlags,
            const unsigned char* data, const size_t& dataSize,
            uint32_t mipLevels);

        uint32_t CreateSampler(const VkSamplerCreateInfo& info);
        uint32_t CreateTexture(uint32_t imageIndex, uint32_t samplerIndex);
        DescriptorImageIndex CreateDescriptorImageInfo(uint32_t textureIndex, uint32_t samplerIndex);

        void CreateTextureDescriptorSet();
        const VkDescriptorSetLayout& GetTextureSetLayout() const;
        const std::vector<VkDescriptorSet>& GetTextureSet() const;

        std::pair<VkImage, VkImageView> GetImage(uint32_t index) const;
        VkSampler GetSampler(uint32_t index) const;
    };
}

#endif // !TEXTURE_MANAGER
