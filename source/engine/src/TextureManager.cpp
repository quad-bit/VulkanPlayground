#include "TextureManager.h"
#include "Assertion.h"
#include "Utils.h"
#include "memory/MemoryManager.h"
#include <basisu/transcoder/basisu_transcoder.h>
#include <filesystem>
#include <fstream>

namespace
{
    void LoadEmptyTexture()
    {
        // toktx --bcmp --genmipmap white.ktx2 .\white.jpg
        basist::basisu_transcoder_init();

        uint32_t mipLevels = 0;
        uint32_t imageIndex = 0;

        // empty image is ktx2
        bool isKtx2 = true;
        VkFormat vulkanFormat = Loops::TextureManager::GetInstance()->GetBestFormat(Loops::TEXTURE_TYPE::DIFFUSE, isKtx2);

        if (isKtx2)
        {
            // Image is KTX2 using basis universal compression. Those images need to be loaded from disk and will be transcoded to a native GPU format
            basist::ktx2_transcoder ktxTranscoder;
            //const std::string filename = std::string{ ASSETS_PATH } + "/textures/test.ktx2";
            const std::string filename = std::string{ ASSETS_PATH } + "/textures/white.ktx2";
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

            imageIndex = Loops::TextureManager::GetInstance()->CreateVulkanImage(width,
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

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.compareOp = VK_COMPARE_OP_NEVER;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        samplerInfo.maxLod = (float)mipLevels;
        samplerInfo.maxAnisotropy = 8.0f;
        samplerInfo.anisotropyEnable = VK_TRUE;
        const uint32_t samplerIndex = Loops::TextureManager::GetInstance()->CreateSampler(samplerInfo);

        // a sampler combined with image forms a texture
        // creating a unique sampler for every gltf texture(sampler+image)
        auto textureIndex = Loops::TextureManager::GetInstance()->CreateTexture(imageIndex, samplerIndex);
    }

}

// Initialize static members
Loops::TextureManager* Loops::TextureManager::s_instancePtr = nullptr;
std::mutex Loops::TextureManager::s_mtx;

Loops::TextureManager* Loops::TextureManager::GetInstance()
{
    if (s_instancePtr == nullptr)
    {
        std::lock_guard<std::mutex> lock(s_mtx);
        if (s_instancePtr == nullptr)
        {
            s_instancePtr = new Loops::TextureManager();
        }
    }
    return s_instancePtr;
}

void Loops::TextureManager::DeInitPrivate()
{
    for (auto& [index, vulkanImage] : m_imageList)
    {
        vmaDestroyImage(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), vulkanImage.m_vkImage, vulkanImage.m_vmaAllocation);
        vkDestroyImageView(m_device, vulkanImage.m_vkImageView, nullptr);
    }

    for (auto& [index, sampler] : m_samplerMap)
    {
        vkDestroySampler(m_device, sampler, nullptr);
    }

    vkDestroyDescriptorPool(m_device, m_textureDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_textureDescriptorSetLayout, nullptr);
}

void Loops::TextureManager::Init(const VkPhysicalDevice& physicalDevice,
    const VkDevice& device, const VkQueue& queue,
    uint32_t queuefamilyIndex, uint32_t maxFrameInFlights)
{
    // find the available formats
    m_physicalDevice = physicalDevice;
    m_device = device;
    m_queue = queue;
    m_queueFamilyIndex = queuefamilyIndex;
    m_maxFrameInFlights = maxFrameInFlights;

    VkFormatProperties props = {};

    for (auto& item : m_preferredCompressedTextureFormatMapPC)
    {
        const TEXTURE_TYPE& mapType = item.first;
        const std::vector<VkFormat>& preferredFormats = item.second;

        for (auto& format : preferredFormats)
        {
            vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);
            if (props.optimalTilingFeatures & m_textureFeatureMap.at(mapType))
            {
                m_availableCompressedFormats.insert({ mapType, format });
                break;
            }
        }
    }

    for (auto& item : m_preferredUncompressedTextureFormatMapPC)
    {
        const TEXTURE_TYPE& mapType = item.first;
        const std::vector<VkFormat>& preferredFormats = item.second;

        for (auto& format : preferredFormats)
        {
            vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);
            if (props.optimalTilingFeatures & m_textureFeatureMap.at(mapType))
            {
                m_availableUncompressedFormats.insert({ mapType, format });
                break;
            }
        }
    }

    LoadEmptyTexture();
}

bool Loops::TextureManager::IsFormatAvailable(const VkFormat& format, const VkFormatFeatureFlags& formatFeature) const
{
    VkFormatProperties props{};
    vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &props);
    if ((props.optimalTilingFeatures & formatFeature) && 
        (props.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) &&
        (props.optimalTilingFeatures & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT))
    {
        return true;
    }
    return false;
}

VkFormat Loops::TextureManager::GetBestFormat(const TEXTURE_TYPE& textureType, bool isCompressed) const
{
    if (isCompressed)
    {
        auto it = m_availableCompressedFormats.find(textureType);
        ASSERT_MSG(it != m_availableCompressedFormats.end(), "texture type not found");
        return it->second;
    }
    else
    {
        auto it = m_availableUncompressedFormats.find(textureType);
        ASSERT_MSG(it != m_availableUncompressedFormats.end(), "texture type not found");
        return it->second;
    }
    ASSERT_MSG_DEBUG(0, "format not found");
    return VkFormat{};
}

uint32_t Loops::TextureManager::CreateVulkanImage(const uint32_t width, 
    const uint32_t height, const VkFormat& format,
    const VkImageUsageFlags& usageFlags, const unsigned char* data,
    const size_t& dataSize, uint32_t mipLevels,
    const std::vector<MipInfo>& mipInfoList,
    const uint32_t& bytesPerBlockOrPixel)
{
    auto vulkanImage = Loops::VkUtils::CreateImageVma(m_physicalDevice,
        m_device,
        Memory::MemoryManager::GetInstance()->GetVmaAllocator(),
        width,
        height,
        format,
        usageFlags | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        mipLevels,
        VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT
        );

    uint32_t textureIndex = m_imageCount++;
    m_imageList.insert({ textureIndex, vulkanImage });
    auto [stagingBuffer, stagingBufferMemory] = VkUtils::LoadImageDataIntoStagingBuffer(m_physicalDevice,
        m_device, data,
        dataSize);

    {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandPoolCreateInfo info{};
        info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        info.pNext = nullptr;
        info.queueFamilyIndex = m_queueFamilyIndex;
        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

        VkUtils::ErrorCheck(vkCreateCommandPool(m_device, &info, nullptr, &pool));

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.commandBufferCount = 1;
        allocInfo.commandPool = pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.pNext = nullptr;
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

        VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
        VkUtils::ErrorCheck(vkAllocateCommandBuffers(m_device, &allocInfo, &cmdBuffer));

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        beginInfo.pInheritanceInfo = nullptr;
        beginInfo.pNext = nullptr;
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkBeginCommandBuffer(cmdBuffer, &beginInfo);
        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.levelCount = mipLevels;
        subresourceRange.layerCount = 1;

        VkImageMemoryBarrier imageMemoryBarrier{};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrier.srcAccessMask = 0;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.image = vulkanImage.m_vkImage;
        imageMemoryBarrier.subresourceRange = subresourceRange;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);

        // Transcode and copy all image levels
        VkDeviceSize bufferOffset = 0;
        for (uint32_t i = 0; i < mipLevels; i++)
        {
            // Size calculations differ for compressed/uncompressed formats
            uint32_t outputSize = mipInfoList[i].numBlocksOrPixels * bytesPerBlockOrPixel;

            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = i;
            bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = mipInfoList[i].width;
            bufferCopyRegion.imageExtent.height = mipInfoList[i].height;
            bufferCopyRegion.imageExtent.depth = 1;
            bufferCopyRegion.bufferOffset = bufferOffset;

            vkCmdCopyBufferToImage(cmdBuffer, stagingBuffer, vulkanImage.m_vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

            bufferOffset += outputSize;
        }

        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        imageMemoryBarrier.image = vulkanImage.m_vkImage;
        imageMemoryBarrier.subresourceRange = subresourceRange;
        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        vkEndCommandBuffer(cmdBuffer);

        VkFence fence = VK_NULL_HANDLE;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.pNext = nullptr;
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkUtils::ErrorCheck(vkCreateFence(m_device, &fenceInfo, nullptr, &fence));

        VkSubmitInfo submitInfo{};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;
        submitInfo.pNext = nullptr;
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkUtils::ErrorCheck(vkQueueSubmit(m_queue, 1, &submitInfo, fence));

        VkUtils::ErrorCheck(vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX));

        vkDestroyFence(m_device, fence, nullptr);
        vkDestroyCommandPool(m_device, pool, nullptr);
    }

    vkFreeMemory(m_device, stagingBufferMemory, nullptr);
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);

    return textureIndex;
}

uint32_t Loops::TextureManager::CreateVulkanImage(const uint32_t width, const uint32_t height,
    const VkFormat& format, const VkImageUsageFlags& usageFlags,
    const unsigned char* data, const size_t& dataSize,
    uint32_t mipLevels)
{
    auto vulkanImage = Loops::VkUtils::CreateImageVma(m_physicalDevice,
        m_device,
        Memory::MemoryManager::GetInstance()->GetVmaAllocator(),
        width,
        height,
        format,
        usageFlags | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        mipLevels,
        VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT
    );

    uint32_t textureIndex = m_imageCount++;
    m_imageList.insert({ textureIndex, vulkanImage });

    auto [stagingBuffer, stagingBufferMemory] = VkUtils::LoadImageDataIntoStagingBuffer(m_physicalDevice,
        m_device, data,
        dataSize);

    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.pNext = nullptr;
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkUtils::ErrorCheck(vkCreateFence(m_device, &fenceInfo, nullptr, &fence));

    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo info{};
    info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.pNext = nullptr;
    info.queueFamilyIndex = m_queueFamilyIndex;
    info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    VkUtils::ErrorCheck(vkCreateCommandPool(m_device, &info, nullptr, &pool));

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.commandBufferCount = 1;
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.pNext = nullptr;
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

    VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
    VkUtils::ErrorCheck(vkAllocateCommandBuffers(m_device, &allocInfo, &cmdBuffer));

    // change layout (undefined -> dst) -> copy to mip level 0 -> change layout (dst -> src)
    {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
        beginInfo.pInheritanceInfo = nullptr;
        beginInfo.pNext = nullptr;
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkBeginCommandBuffer(cmdBuffer, &beginInfo);

        {
            VkImageSubresourceRange subresourceRange = {};
            subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            subresourceRange.levelCount = 1;
            subresourceRange.layerCount = 1;

            {
                VkImageMemoryBarrier imageMemoryBarrier{};
                imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageMemoryBarrier.srcAccessMask = 0;
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imageMemoryBarrier.image = vulkanImage.m_vkImage;
                imageMemoryBarrier.subresourceRange = subresourceRange;
                vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
            }

            VkBufferImageCopy bufferCopyRegion = {};
            bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            bufferCopyRegion.imageSubresource.mipLevel = 0;
            bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
            bufferCopyRegion.imageSubresource.layerCount = 1;
            bufferCopyRegion.imageExtent.width = width;
            bufferCopyRegion.imageExtent.height = height;
            bufferCopyRegion.imageExtent.depth = 1;

            vkCmdCopyBufferToImage(cmdBuffer, stagingBuffer, vulkanImage.m_vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

            {
                VkImageMemoryBarrier imageMemoryBarrier{};
                imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                imageMemoryBarrier.image = vulkanImage.m_vkImage;
                imageMemoryBarrier.subresourceRange = subresourceRange;
                vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
            }

        vkEndCommandBuffer(cmdBuffer);
        }

        VkSubmitInfo submitInfo{};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;
        submitInfo.pNext = nullptr;
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkUtils::ErrorCheck(vkQueueSubmit(m_queue, 1, &submitInfo, fence));

        VkUtils::ErrorCheck(vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX));
    } // image data copied to mip 0 and mip 0's layout is in transfer src

    VkUtils::ErrorCheck(vkResetFences(m_device, 1, &fence));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    beginInfo.pInheritanceInfo = nullptr;
    beginInfo.pNext = nullptr;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    {
        for (uint32_t i = 1; i < mipLevels; i++)
        {
            VkImageBlit imageBlit{};

            imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageBlit.srcSubresource.layerCount = 1;
            imageBlit.srcSubresource.mipLevel = i - 1;
            imageBlit.srcOffsets[1].x = int32_t(width >> (i - 1));
            imageBlit.srcOffsets[1].y = int32_t(height >> (i - 1));
            imageBlit.srcOffsets[1].z = 1;

            imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imageBlit.dstSubresource.layerCount = 1;
            imageBlit.dstSubresource.mipLevel = i;
            imageBlit.dstOffsets[1].x = int32_t(width >> i);
            imageBlit.dstOffsets[1].y = int32_t(height >> i);
            imageBlit.dstOffsets[1].z = 1;

            VkImageSubresourceRange mipSubRange = {};
            mipSubRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            mipSubRange.baseMipLevel = i;
            mipSubRange.levelCount = 1;
            mipSubRange.layerCount = 1;

            // change individual mip level's layout to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            {
                VkImageMemoryBarrier imageMemoryBarrier{};
                imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
                imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageMemoryBarrier.srcAccessMask = 0;
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imageMemoryBarrier.image = vulkanImage.m_vkImage;
                imageMemoryBarrier.subresourceRange = mipSubRange;
                vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
            }

            vkCmdBlitImage(cmdBuffer, vulkanImage.m_vkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vulkanImage.m_vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);

            {
                VkImageMemoryBarrier imageMemoryBarrier{};
                imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                imageMemoryBarrier.image = vulkanImage.m_vkImage;
                imageMemoryBarrier.subresourceRange = mipSubRange;
                vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
            }
        }

        VkImageSubresourceRange subresourceRange = {};
        subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceRange.layerCount = 1;
        subresourceRange.levelCount = mipLevels;

        {
            VkImageMemoryBarrier imageMemoryBarrier{};
            imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            imageMemoryBarrier.image = vulkanImage.m_vkImage;
            imageMemoryBarrier.subresourceRange = subresourceRange;
            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        }
        vkEndCommandBuffer(cmdBuffer);
    }

    VkSubmitInfo submitInfo{};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;
    submitInfo.pNext = nullptr;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkUtils::ErrorCheck(vkQueueSubmit(m_queue, 1, &submitInfo, fence));
    VkUtils::ErrorCheck(vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX));

    vkDestroyFence(m_device, fence, nullptr);
    vkDestroyCommandPool(m_device, pool, nullptr);
    vkFreeMemory(m_device, stagingBufferMemory, nullptr);
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);

    return textureIndex;
}

uint32_t Loops::TextureManager::CreateSampler(const VkSamplerCreateInfo& info)
{
    VkSampler sampler = VK_NULL_HANDLE;
    VkUtils::ErrorCheck(vkCreateSampler(m_device, &info, nullptr, &sampler));
    m_samplerMap.insert({ m_samplerCounter, sampler });
    return m_samplerCounter++;
}

uint32_t Loops::TextureManager::CreateTexture(uint32_t imageIndex, uint32_t samplerIndex)
{
    m_textureList.insert({ m_textureCount, Texture{imageIndex, samplerIndex} });
    return m_textureCount++;
}

Loops::DescriptorImageIndex Loops::TextureManager::CreateDescriptorImageInfo(uint32_t textureIndex, uint32_t samplerIndex)
{
    const VkImage& image = m_imageList.at(textureIndex).m_vkImage;
    const VkImageView& imageView = m_imageList.at(textureIndex).m_vkImageView;
    const VkSampler& sampler = m_samplerMap[samplerIndex];

    VkDescriptorImageInfo imageInfo{ sampler, imageView, VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
    m_descriptorImageInfoList.insert({ m_descriptorImageInfoCount, imageInfo });
    return m_descriptorImageInfoCount++;
}

void Loops::TextureManager::CreateTextureDescriptorSet()
{
    // Pool creation
    {
        std::vector<VkDescriptorPoolSize> descriptorPoolSize{ 
            {
                VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                m_textureCount * m_maxFrameInFlights
            }
        };

        VkDescriptorPoolCreateInfo descriptorPoolCreateInfo{};
        descriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT;
        descriptorPoolCreateInfo.maxSets = m_maxFrameInFlights;
        descriptorPoolCreateInfo.pNext = nullptr;
        descriptorPoolCreateInfo.poolSizeCount = descriptorPoolSize.size();
        descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSize.data();
        descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;

        Loops::VkUtils::ErrorCheck(vkCreateDescriptorPool(
            m_device, &descriptorPoolCreateInfo, nullptr, &m_textureDescriptorPool));
    }

    // Descriptorset layout
    {
        VkDescriptorSetLayoutBinding binding
        {
            TEXTURE_SET_BINDING_VALUE,//binding location
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            m_textureCount,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            nullptr
        };

        std::vector<VkDescriptorBindingFlagsEXT> descriptorBindingFlags
        {
            VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT
        };

        VkDescriptorSetLayoutBindingFlagsCreateInfoEXT setlayoutBindingFlags
        {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT,
            nullptr,//pNext
            (uint32_t)descriptorBindingFlags.size(),
            descriptorBindingFlags.data()
        };

        VkDescriptorSetLayoutCreateInfo setlayoutCreateInfo
        {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            &setlayoutBindingFlags,//pNext,
            VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
            1,// count
            &binding
        };

        Loops::VkUtils::ErrorCheck(vkCreateDescriptorSetLayout(m_device,
            &setlayoutCreateInfo,
            nullptr,
            &m_textureDescriptorSetLayout
        ));
    }

    // descriptor set creation
    {
        std::vector<uint32_t> variableDesciptorCounts
        {
            m_textureCount
        };
        VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variableAllocInfo
        {
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
            nullptr,
            (uint32_t)variableDesciptorCounts.size(),
            variableDesciptorCounts.data()
        };

        VkDescriptorSetAllocateInfo allocInfo
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = &variableAllocInfo,
            .descriptorPool = m_textureDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &m_textureDescriptorSetLayout
        };

        std::vector<VkDescriptorImageInfo> descriptorImageInfos(m_textureCount);
        for (uint32_t i = 0; i < m_textureCount; i++)
        {
            descriptorImageInfos[i].sampler = m_samplerMap[m_textureList[i].m_samplerIndex];
            descriptorImageInfos[i].imageView = m_imageList[m_textureList[i].m_vulkanImageWrapperIndex].m_vkImageView;
            descriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        VkWriteDescriptorSet writeInfo
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstBinding = TEXTURE_SET_BINDING_VALUE,
            .dstArrayElement = 0,
            .descriptorCount = m_textureCount,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = descriptorImageInfos.data(),
            .pBufferInfo = 0
        };

        m_textureDescriptorSets.resize(m_maxFrameInFlights);
        for (uint32_t i = 0; i < m_maxFrameInFlights; i++)
        {
            VkUtils::ErrorCheck(vkAllocateDescriptorSets(
                m_device,
                &allocInfo,
                &m_textureDescriptorSets[i]
            ));

            writeInfo.dstSet = m_textureDescriptorSets[i];
            vkUpdateDescriptorSets(
                m_device,
                1,
                &writeInfo,
                0,
                nullptr);
        };
    }
}

const VkDescriptorSetLayout& Loops::TextureManager::GetTextureSetLayout() const
{
    return m_textureDescriptorSetLayout;
}

const std::vector<VkDescriptorSet>& Loops::TextureManager::GetTextureSet() const
{
    return m_textureDescriptorSets;
}

std::pair<VkImage, VkImageView> Loops::TextureManager::GetImage(uint32_t index) const
{
    VkImage image = m_imageList.at(index).m_vkImage;
    VkImageView imageView = m_imageList.at(index).m_vkImageView;
    return { image, imageView };
}

VkSampler Loops::TextureManager::GetSampler(uint32_t index) const
{
    VkSampler sampler = m_samplerMap.at(index);
    return sampler;
}

void Loops::TextureManager::DeInit()
{
    s_instancePtr->DeInitPrivate();
    delete s_instancePtr;
    s_instancePtr = nullptr;
}
