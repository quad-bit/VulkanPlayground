#include "effects/TranslucentEffect.h"
#include "TextureManager.h"
#include "MaterialManager.h"
#include "memory/MemoryManager.h"
#include "Utils.h"
#include <plog/Log.h>

Loops::TranslucentEffect::TranslucentEffect(
    const Loops::VkUtils::VulkanContext * const vulkanContext,
    const std::vector<VkDescriptorSet>& sceneSets,
    const std::vector<VkDescriptorSet>& transformSets,
    const VkDescriptorSetLayout& sceneSetLayout,
    const VkDescriptorSetLayout& transformSetLayout,
    const std::vector<VkImageView>& colorTargetViews,
    const std::vector<VkImageView>& depthTargetViews,
    const MaterialManager* pMaterialManager,
    const Loops::SceneManager* pSceneManager) :
    m_sceneSets(sceneSets), m_transformSets(transformSets),
    m_vulkanContext(vulkanContext), m_mutexList(vulkanContext->m_maxFrameInFlights),
    m_signalAtomics(vulkanContext->m_maxFrameInFlights)
{
    auto CreateImage = [this, vulkanContext](uint32_t width, uint32_t height,
        const VkFormat& format, VkImageUsageFlags usageFlags,
        uint32_t mipLevels, VkImageAspectFlags aspect) -> Loops::VulkanImage
        {
            auto vulkanImage = Loops::VkUtils::CreateImageVma(vulkanContext->m_physicalDevice,
                vulkanContext->m_logicalDevice,
                Memory::MemoryManager::GetInstance()->GetVmaAllocator(),
                width,
                height,
                format,
                usageFlags,// | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                mipLevels,
                aspect
            );

            return vulkanImage;
        };

    const VkFormat formats[2]{
            Loops::TextureManager::GetInstance()->GetBestFormat(Loops::TEXTURE_TYPE::FBO, false),
            Loops::TextureManager::GetInstance()->GetBestFormat(Loops::TEXTURE_TYPE::DEPTH_STENCIL, false),
            //VK_FORMAT_D32_SFLOAT
    };

    const uint32_t mipLevels{ 1 };
    const uint32_t numUniforms = vulkanContext->m_maxFrameInFlights;
    m_opaquePassColorTargetCopy.resize(numUniforms);
    m_opaquePassDepthTargetCopy.resize(numUniforms);
    std::vector<VkImage> colorCopyImages, depthCopyImages;
    std::vector<VkImageView> colorCopyImageViews, depthCopyImageViews;

    VkImageAspectFlags aspect{ VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT };
    if (formats[1] == VK_FORMAT_D32_SFLOAT_S8_UINT ||
        formats[1] == VK_FORMAT_D24_UNORM_S8_UINT)
        aspect |= VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT;

    // NOTE
    /*
    * 1:Error
    * Format VK_FORMAT_D32_SFLOAT_S8_UINT
    * View creation Aspect VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT|VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT
    * Image layout change Aspect VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT|VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT
    * Causes VKUpdateDescriptorSet to throw: both the bits depth&stencil cannot be set, only one is allowed
    * 
    * 2:Error
    * Format VK_FORMAT_D32_SFLOAT_S8_UINT
    * View creation Aspect VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT
    * Image layout change Aspect VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT
    * Causes PipelineBarrier to throw: Format has depthStencil but the aspect doesn't
    * 
    * 3:Working
    * Format VK_FORMAT_D32_SFLOAT_S8_UINT
    * View creation Aspect VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT
    * Image layout change Aspect VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT
    * 
    * 3:Working
    * Format VK_FORMAT_D32_SFLOAT
    * View creation Aspect VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT
    * Image layout change Aspect VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT
    */

    for (uint32_t i = 0; i < numUniforms; i++)
    {
        m_opaquePassColorTargetCopy[i] = CreateImage(
            vulkanContext->m_renderDimensions.m_width,
            vulkanContext->m_renderDimensions.m_height,
            Loops::TextureManager::GetInstance()->GetBestFormat(Loops::TEXTURE_TYPE::DIFFUSE, false),//formats[0],
            VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            mipLevels,
            VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT
        );
        colorCopyImages.push_back(m_opaquePassColorTargetCopy[i].m_vkImage);
        colorCopyImageViews.push_back(m_opaquePassColorTargetCopy[i].m_vkImageView);

        m_opaquePassDepthTargetCopy[i] = CreateImage(
            vulkanContext->m_renderDimensions.m_width,
            vulkanContext->m_renderDimensions.m_height,
            formats[1],
            //VkImageUsageFlagBits::VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
            VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT |
            VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            mipLevels,
            VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT// not adding stencil here see above note
        );
        depthCopyImages.push_back(m_opaquePassDepthTargetCopy[i].m_vkImage);
        depthCopyImageViews.push_back(m_opaquePassDepthTargetCopy[i].m_vkImageView);
    }

    VkUtils::ChangeImageLayout(vulkanContext->m_logicalDevice,
        colorCopyImages, vulkanContext->m_graphicsQueue,
        vulkanContext->m_graphicsQueueFamilyIndex,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT);

    VkUtils::ChangeImageLayout(vulkanContext->m_logicalDevice,
        depthCopyImages, vulkanContext->m_graphicsQueue,
        vulkanContext->m_graphicsQueueFamilyIndex,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        aspect);

    // depth render task
    {
        std::vector< std::pair<Loops::EFFECT_TYPE, Loops::TECHNIQUE_TYPE>> targetPasses{ {Loops::EFFECT_TYPE::TRANSLUCENT_EFT, Loops::TECHNIQUE_TYPE::VOLUME_TRANSMISSION} };
        const bool allocateCommandBuffers{ true };
        mp_depthRenderTask = std::make_unique<Tasking::DepthRenderTask>(
            m_vulkanContext,
            transformSetLayout,
            sceneSetLayout,
            sceneSets,
            transformSets,
            formats[1],
            numUniforms,
            VkClearDepthStencilValue{ 1.0f, 0 },
            targetPasses,
            allocateCommandBuffers,
            VK_CULL_MODE_FRONT_BIT
        );
    }

    // transmission volume task
    {
        std::vector<VkImageView> backDepthImageViews = mp_depthRenderTask->GetDepthTargetViews();
        const bool allocateCommandBuffers{ true };

        mp_transmissionVolumeTask = std::make_unique<Tasking::TransmissionVolumeTask>(
            m_vulkanContext,
            vulkanContext->m_graphicsQueueFamilyIndex,
            sceneSets,
            transformSets,
            sceneSetLayout,
            transformSetLayout,
            colorCopyImageViews,
            depthCopyImageViews,
            backDepthImageViews,
            colorTargetViews,
            depthTargetViews,
            formats[0], formats[1],
            pMaterialManager,
            allocateCommandBuffers
        );
    }

    {
        auto TransitionImageToTransferSrc = [](VkCommandBuffer& cmdBuffer, VkImage& image,
            const bool& isColor)
            {
                VkImageMemoryBarrier2 imageBarrier{};
                imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                imageBarrier.pNext = nullptr;

                // Define synchronization stages
                imageBarrier.srcStageMask = isColor ? VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
                imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;

                // Define memory access masks
                imageBarrier.srcAccessMask = isColor ? VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                imageBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;

                // Define layout transitions
                imageBarrier.oldLayout = isColor ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

                // Define resource aspects (Color Attachment)
                imageBarrier.image = image;
                imageBarrier.subresourceRange.aspectMask = isColor ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;;
                imageBarrier.subresourceRange.baseMipLevel = 0;
                imageBarrier.subresourceRange.levelCount = 1;
                imageBarrier.subresourceRange.baseArrayLayer = 0;
                imageBarrier.subresourceRange.layerCount = 1;

                // Queue family ownership transfer (ignored here)
                imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

                VkDependencyInfo dependencyInfo{};
                dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                dependencyInfo.pNext = nullptr;
                dependencyInfo.dependencyFlags = 0;
                dependencyInfo.imageMemoryBarrierCount = 1;
                dependencyInfo.pImageMemoryBarriers = &imageBarrier;

                // Execute the pipeline barrier
                vkCmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
            };

        auto TransitionImageFromTransferSrc = [](VkCommandBuffer& cmdBuffer, VkImage& image, const bool& isColor)
            {
                VkImageMemoryBarrier2 imageBarrier{};
                imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                imageBarrier.pNext = nullptr;

                // Define the synchronization scopes
                imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                imageBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
                imageBarrier.dstStageMask = isColor ? VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
                imageBarrier.dstAccessMask = isColor ? VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT : VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                // Define the layout transition
                imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                imageBarrier.newLayout = isColor ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

                // Keep family ownership unchanged
                imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

                // Target image details
                imageBarrier.image = image;
                imageBarrier.subresourceRange.aspectMask = isColor ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
                imageBarrier.subresourceRange.baseMipLevel = 0;
                imageBarrier.subresourceRange.levelCount = 1;
                imageBarrier.subresourceRange.baseArrayLayer = 0;
                imageBarrier.subresourceRange.layerCount = 1;

                VkDependencyInfo dependencyInfo{};
                dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                dependencyInfo.pNext = nullptr;
                dependencyInfo.dependencyFlags = 0;
                dependencyInfo.imageMemoryBarrierCount = 1;
                dependencyInfo.pImageMemoryBarriers = &imageBarrier;

                vkCmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
            };

        auto TransitionImageFromTransferDstToShaderRead = [](VkCommandBuffer& cmdBuffer, VkImage& image, const bool& isColor)
            {
                VkImageMemoryBarrier2 imageBarrier{};
                imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                imageBarrier.pNext = nullptr;

                // Define the synchronization stages
                imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT; // Source: Transfer/Copy operation
                imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT; // Destination: Shader reading (e.g., Fragment shader)

                // Define the access masks (caches to flush/invalidate)
                imageBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT; // Flush transfer writes
                imageBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT; // Invalidate shader reads

                // Layout transitions
                imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                // Ownership queue family transfers (ignored if not changing queues)
                imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

                // Define the subresource range (which parts of the image to transition)
                imageBarrier.image = image;
                imageBarrier.subresourceRange.aspectMask = isColor ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT| VK_IMAGE_ASPECT_STENCIL_BIT;
                imageBarrier.subresourceRange.baseMipLevel = 0;
                imageBarrier.subresourceRange.levelCount = mipLevels;
                imageBarrier.subresourceRange.baseArrayLayer = 0;
                imageBarrier.subresourceRange.layerCount = 1;

                // Package the barrier into dependency info
                VkDependencyInfo dependencyInfo{};
                dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                dependencyInfo.pNext = nullptr;
                dependencyInfo.dependencyFlags = 0;
                dependencyInfo.imageMemoryBarrierCount = 1;
                dependencyInfo.pImageMemoryBarriers = &imageBarrier;

                // Record the pipeline barrier
                vkCmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
            };

        auto TransitionImageFromShaderReadToTransferDst = [](VkCommandBuffer& cmdBuffer, VkImage& image, const bool& isColor)
            {
                VkImageMemoryBarrier2 imageBarrier{};
                imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
                imageBarrier.pNext = nullptr;

                // Define synchronization stages
                imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT; // Adjust if read by vertex/compute
                imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;

                // Define access masks
                imageBarrier.srcAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
                imageBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;

                // Define layout transition
                imageBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

                // Keep ownership family indices identical to ignore queue family transfers
                imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

                // Target image and its subresource range
                imageBarrier.image = image;
                imageBarrier.subresourceRange.aspectMask = isColor ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
                imageBarrier.subresourceRange.baseMipLevel = 0;
                imageBarrier.subresourceRange.levelCount = mipLevels;
                imageBarrier.subresourceRange.baseArrayLayer = 0;
                imageBarrier.subresourceRange.layerCount = 1;

                VkDependencyInfo dependencyInfo{};
                dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                dependencyInfo.pNext = nullptr;
                dependencyInfo.dependencyFlags = 0;
                dependencyInfo.imageMemoryBarrierCount = 1;
                dependencyInfo.pImageMemoryBarriers = &imageBarrier;

                // Record the pipeline barrier
                vkCmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
            };

        auto BlitImage = [](VkCommandBuffer& cmdBuffer, VkImage& srcImage, VkImage& dstImage,
            int width, int height,
            const bool& isColor)
            {
                // Define the region to blit (entire image)
                VkImageBlit blitRegion{};

                // Source offset and dimensions
                blitRegion.srcSubresource.aspectMask = isColor ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;;
                blitRegion.srcSubresource.mipLevel = 0;
                blitRegion.srcSubresource.baseArrayLayer = 0;
                blitRegion.srcSubresource.layerCount = 1;
                blitRegion.srcOffsets[0] = { 0, 0, 0 };
                blitRegion.srcOffsets[1] = { width, height, 1 };

                // Destination offset and dimensions (can be different for scaling)
                blitRegion.dstSubresource.aspectMask = isColor ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;;
                blitRegion.dstSubresource.mipLevel = 0;
                blitRegion.dstSubresource.baseArrayLayer = 0;
                blitRegion.dstSubresource.layerCount = 1;
                blitRegion.dstOffsets[0] = { 0, 0, 0 };
                blitRegion.dstOffsets[1] = { width, height, 1 };

                // Record the blit command into the passed command buffer
                vkCmdBlitImage(
                    cmdBuffer,
                    srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,  // Src image and layout
                    dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,  // Dst image and layout
                    1, &blitRegion,
                    isColor ? VK_FILTER_LINEAR : VK_FILTER_NEAREST   // Filter for blending/scaling
                );
            };

        m_frameData.m_taskDataList.resize(vulkanContext->m_maxFrameInFlights);

        // copy task
        // change layout opaque targets to transferSrc
        // change layout of copy images to transferDst
        // copy opaque color and depth target 
        // change layout opaque targets to Color and depth attachment
        // change layout of copy images to shader read only

        auto OpaqueGrabTask = [this, &TransitionImageFromTransferSrc, &TransitionImageToTransferSrc,
        &TransitionImageFromTransferDstToShaderRead, &TransitionImageFromShaderReadToTransferDst, 
        &BlitImage]()
            {
                const auto& currentFrameInFlight = m_frameData.m_currentFrameInFlight;
                auto& taskData = m_frameData.m_taskDataList[currentFrameInFlight];

                VkImage& colorImage = taskData.m_opaqueColorImage;
                VkImage& depthImage = taskData.m_opaqueDepthImage;

                VkImage& colorCopy = m_opaquePassColorTargetCopy[currentFrameInFlight].m_vkImage;
                VkImage& depthCopy = m_opaquePassDepthTargetCopy[currentFrameInFlight].m_vkImage;

                VkCommandBuffer& cmdBuf = m_opaqueGrabCommandBuffers[currentFrameInFlight];

                Loops::VkUtils::ErrorCheck(vkResetCommandBuffer(cmdBuf, 0));

                VkCommandBufferBeginInfo beginInfo{};
                beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                Loops::VkUtils::ErrorCheck(vkBeginCommandBuffer(cmdBuf, &beginInfo));

                // opaque targets
                TransitionImageToTransferSrc(cmdBuf, colorImage, true);
                TransitionImageToTransferSrc(cmdBuf, depthImage, false);

                // local image copies
                TransitionImageFromShaderReadToTransferDst(cmdBuf, colorCopy, true);
                TransitionImageFromShaderReadToTransferDst(cmdBuf, depthCopy, false);

                // Copy
                BlitImage(cmdBuf, colorImage, colorCopy, m_vulkanContext->m_renderDimensions.m_width, m_vulkanContext->m_renderDimensions.m_height, true);
                BlitImage(cmdBuf, depthImage, depthCopy, m_vulkanContext->m_renderDimensions.m_width, m_vulkanContext->m_renderDimensions.m_height, false);

                TransitionImageFromTransferDstToShaderRead(cmdBuf, colorCopy, true);
                TransitionImageFromTransferDstToShaderRead(cmdBuf, depthCopy, false);

                // opaque targets
                TransitionImageFromTransferSrc(cmdBuf, colorImage, true);
                TransitionImageFromTransferSrc(cmdBuf, depthImage, false);

                Loops::VkUtils::ErrorCheck(vkEndCommandBuffer(cmdBuf));

                {
                    std::lock_guard lock(m_mutexList[currentFrameInFlight]);

                    const uint64_t waitValue = m_frameData.m_waitValue;
                    const uint64_t signalValue = ++m_signalAtomics[currentFrameInFlight];

                    VkSemaphoreSubmitInfo waitInfo{};
                    //if (waitValue.has_value())
                    {
                        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                        waitInfo.pNext = nullptr;
                        waitInfo.semaphore = m_frameData.m_semaphore;
                        waitInfo.stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                        waitInfo.deviceIndex = 0;
                        waitInfo.value = waitValue;
                    };

                    VkSemaphoreSubmitInfo signalInfo
                    { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, nullptr, m_frameData.m_semaphore, signalValue, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0 };

                    VkCommandBufferSubmitInfo bufInfo{};
                    bufInfo.commandBuffer = cmdBuf;
                    bufInfo.deviceMask = 0;
                    bufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;

                    VkSubmitInfo2 submitInfo{};
                    submitInfo.commandBufferInfoCount = 1;
                    submitInfo.pCommandBufferInfos = &bufInfo;
                    submitInfo.pSignalSemaphoreInfos = &signalInfo;
                    submitInfo.signalSemaphoreInfoCount = 1;
                    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
                    //if (waitValue.has_value())
                    {
                        submitInfo.waitSemaphoreInfoCount = 1;
                        submitInfo.pWaitSemaphoreInfos = &waitInfo;
                    }

                    // If the threads are being killed, we need to skip the queue submission to allow the program to exit gracefully
                    //if (m_alive)
                    {
                        Loops::VkUtils::ErrorCheck(vkQueueSubmit2(m_vulkanContext->m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
                    }
                }
            };

        // depth only render task 
        // mp_depthRenderTask->Update();
        auto BackFaceDepthUpdateTask = [this, pSceneManager, pMaterialManager]()
            {
                const auto& currentFrameInFlight = m_frameData.m_currentFrameInFlight;
                auto& taskData = m_frameData.m_taskDataList[currentFrameInFlight];

                const uint64_t waitValue = m_frameData.m_waitValue;

                mp_depthRenderTask->Update(currentFrameInFlight,
                    m_frameData.m_semaphore,
                    waitValue,
                    pSceneManager->GetRenderData(currentFrameInFlight),
                    pSceneManager, pMaterialManager->GetSceneMaterials(),
                    pSceneManager->GetTransformDescriptorSet(currentFrameInFlight),
                    m_mutexList[currentFrameInFlight], m_signalAtomics[currentFrameInFlight]);
            };

        // transmission volume render task
        auto TransmissionTask = [this, pSceneManager, pMaterialManager]()
            {
                const auto& currentFrameInFlight = m_frameData.m_currentFrameInFlight;
                auto& taskData = m_frameData.m_taskDataList[currentFrameInFlight];

                const uint64_t waitValue = m_frameData.m_waitValue + 2;
                const uint64_t signalValue = m_frameData.m_waitValue + 3;

                /*
                const uint32_t& frameInFlight, const VkSemaphore& timelineSem,
                uint64_t signalValue, std::optional<uint64_t> waitValue,
                const Loops::RenderData& renderData,
                const Loops::SceneManager& sceneManager,
                const std::unordered_map<uint32_t, Loops::Material>& materials,
                const VkDescriptorSet& transformSet,
                const VkDescriptorSet& sceneSet
                */
                mp_transmissionVolumeTask->Update(currentFrameInFlight,
                    m_frameData.m_semaphore,
                    signalValue, waitValue,
                    pSceneManager->GetRenderData(currentFrameInFlight),
                    *pSceneManager, pMaterialManager->GetSceneMaterials(), 
                    pSceneManager->GetTransformDescriptorSet(currentFrameInFlight),
                    m_frameData.m_sceneSet);
            };

        m_taskflows.resize(m_vulkanContext->m_maxFrameInFlights);
        for(auto& taskflow : m_taskflows)
        {
            tf::Task opqGrabTfTask = taskflow.emplace(OpaqueGrabTask).name("OpaqueGrabTask");
            tf::Task backFaceDepthTfTask = taskflow.emplace(BackFaceDepthUpdateTask).name("BackFaceDepthUpdateTask");
            tf::Task transVolTfTask = taskflow.emplace(TransmissionTask).name("TransmissionTask");
            // NOTE: to make it serial uncomment the below
            //backFaceDepthTfTask.succeed(opqGrabTfTask);
            transVolTfTask.succeed(backFaceDepthTfTask, backFaceDepthTfTask);
        }
    }

    {
        VkCommandPoolCreateInfo createInfo{};
        createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        createInfo.queueFamilyIndex = m_vulkanContext->m_graphicsQueueFamilyIndex;
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

        Loops::VkUtils::ErrorCheck(vkCreateCommandPool(m_vulkanContext->m_logicalDevice, &createInfo, nullptr, &m_commandPool));

        m_opaqueGrabCommandBuffers.resize(m_vulkanContext->m_maxFrameInFlights);
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.commandBufferCount = m_vulkanContext->m_maxFrameInFlights;
        alloc_info.commandPool = m_commandPool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

        Loops::VkUtils::ErrorCheck(vkAllocateCommandBuffers(m_vulkanContext->m_logicalDevice, &alloc_info, &m_opaqueGrabCommandBuffers[0]));
    }
}

uint64_t Loops::TranslucentEffect::Update(const uint32_t& frameInFlight,
    const VkSemaphore& timelineSem, std::optional<uint64_t> waitValue,
    const Loops::RenderData& renderData,
    const Loops::SceneManager* sceneManager,
    const std::unordered_map<uint32_t, Loops::Material>& materials,
    VkImage& opaqueColorImage, VkImage& opaqueDepthImage,
    const VkDescriptorSet& sceneSet)
{
    m_frameData.m_currentFrameInFlight = frameInFlight;
    m_frameData.m_semaphore = timelineSem;
    m_frameData.m_waitValue = waitValue.value();
    m_frameData.m_taskDataList[frameInFlight].m_opaqueColorImage = opaqueColorImage;
    m_frameData.m_taskDataList[frameInFlight].m_opaqueDepthImage= opaqueDepthImage;
    m_frameData.m_sceneSet = sceneSet;

    m_signalAtomics[frameInFlight].store(waitValue.value());

    m_executor.run(m_taskflows[frameInFlight]).wait();

    return waitValue.value() + 1;
}

Loops::TranslucentEffect::~TranslucentEffect()
{
    vkDestroyCommandPool(m_vulkanContext->m_logicalDevice, m_commandPool, nullptr);

    for (auto& image : m_opaquePassColorTargetCopy)
    {
        vmaDestroyImage(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), image.m_vkImage, image.m_vmaAllocation);
        vkDestroyImageView(m_vulkanContext->m_logicalDevice, image.m_vkImageView, nullptr);
    }

    for (auto& image : m_opaquePassDepthTargetCopy)
    {
        vmaDestroyImage(Memory::MemoryManager::GetInstance()->GetVmaAllocator(), image.m_vkImage, image.m_vmaAllocation);
        vkDestroyImageView(m_vulkanContext->m_logicalDevice, image.m_vkImageView, nullptr);
    }

    mp_depthRenderTask.reset();
    mp_transmissionVolumeTask.reset();
}
