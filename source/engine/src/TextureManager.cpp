#include "TextureManager.h"
#include "Assertion.h"
#include "Utils.h"
#include "memory/MemoryManager.h"

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
}

void Loops::TextureManager::Init(const VkPhysicalDevice& physicalDevice,
    const VkDevice& device, const VkQueue& queue,
    uint32_t queuefamilyIndex)
{
    // find the available formats
    m_physicalDevice = physicalDevice;
    m_device = device;
    m_queue = queue;
    m_queueFamilyIndex = queuefamilyIndex;

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

void Loops::TextureManager::DeInit()
{
    s_instancePtr->DeInitPrivate();
    delete s_instancePtr;
    s_instancePtr = nullptr;
}
