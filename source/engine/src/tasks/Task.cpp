#include "tasks/Task.h"
#include "Assertion.h"

void Loops::Tasking::GraphicsTask::CreateAttachments(uint32_t numColorTargets, uint32_t numDepthTargets, const VkFormat& colorFormat, const std::optional<VkFormat>& depthFormat,
    const VkClearColorValue& clearColorValue, const std::optional<VkClearDepthStencilValue>& depthStencilClearValue)
{
    m_taskResource = TaskOwnedResource{};
    TaskOwnedResource& resource = std::get<TaskOwnedResource>(m_taskResource);
    VkClearDepthStencilValue depthClearValue = depthStencilClearValue.has_value() ? depthStencilClearValue.value() : VkClearDepthStencilValue{1.0f, 0};
    VkFormat depthFormatValue = depthFormat.has_value() ? depthFormat.value() : VK_FORMAT_D32_SFLOAT_S8_UINT;
    resource.m_colorTargets.resize(numColorTargets);
    resource.m_depthTargets.resize(numDepthTargets);
    m_renderInfoList = VkUtils::CreateRendertargets(resource.m_colorTargets.data(), resource.m_colorTargets.size(), resource.m_depthTargets.data(),
        resource.m_depthTargets.size(), colorFormat, depthFormatValue, m_vulkanContext->m_renderDimensions.m_width, m_vulkanContext->m_renderDimensions.m_height,
        m_vulkanContext->m_logicalDevice, clearColorValue, depthClearValue, m_colorInfoList, m_depthInfoList);

    for (auto& target : resource.m_colorTargets)
    {
        std::vector<VkImage> images{ target.m_vkImage };
        VkUtils::ChangeImageLayout(m_vulkanContext->m_logicalDevice, images, m_vulkanContext->m_graphicsQueue, m_vulkanContext->m_graphicsQueueFamilyIndex, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT);
    }

    for (auto& target : resource.m_depthTargets)
    {
        std::vector<VkImage> images{ target.m_vkImage };
        VkUtils::ChangeImageLayout(m_vulkanContext->m_logicalDevice, images, m_vulkanContext->m_graphicsQueue, m_vulkanContext->m_graphicsQueueFamilyIndex, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT | VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT);
    }
}

void Loops::Tasking::GraphicsTask::CreateAttachments(uint32_t numDepthTargets,
    const VkFormat& depthFormat,
    const VkClearDepthStencilValue& depthStencilClearValue)
{
    const VkFormat tempColorFormat{ VK_FORMAT_B8G8R8A8_UNORM };
    const VkClearColorValue tempColorValue{};

    m_taskResource = TaskOwnedResource{};
    TaskOwnedResource& resource = std::get<TaskOwnedResource>(m_taskResource);
    resource.m_depthTargets.resize(numDepthTargets);
    m_renderInfoList = VkUtils::CreateRendertargets(
        nullptr, 0,
        resource.m_depthTargets.data(), resource.m_depthTargets.size(),
        tempColorFormat, depthFormat,
        m_vulkanContext->m_renderDimensions.m_width, m_vulkanContext->m_renderDimensions.m_height,
        m_vulkanContext->m_logicalDevice, tempColorValue, depthStencilClearValue,
        m_colorInfoList, m_depthInfoList);

    VkImageAspectFlags aspect{ VkImageAspectFlagBits::VK_IMAGE_ASPECT_DEPTH_BIT };

    if (depthFormat == VkFormat::VK_FORMAT_D32_SFLOAT_S8_UINT ||
        depthFormat == VkFormat::VK_FORMAT_D24_UNORM_S8_UINT)
        aspect |= VkImageAspectFlagBits::VK_IMAGE_ASPECT_STENCIL_BIT;

    for (auto& target : resource.m_depthTargets)
    {
        std::vector<VkImage> images{ target.m_vkImage };
        VkUtils::ChangeImageLayout(m_vulkanContext->m_logicalDevice,
            images, m_vulkanContext->m_graphicsQueue,
            m_vulkanContext->m_graphicsQueueFamilyIndex,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            aspect);
    }
}

Loops::Tasking::GraphicsTask::GraphicsTask(const char* name, const VkUtils::VulkanContext* const vulkanContext, uint32_t numColorTargets, uint32_t numDepthTargets, const VkFormat& colorFormat,
    const std::optional<VkFormat>& depthFormat, const VkClearColorValue& clearColorValue, const std::optional<VkClearDepthStencilValue>& depthStencilClearValue) :
    m_vulkanContext(vulkanContext), m_ownAttachments(true), m_colorFormat(colorFormat), m_depthFormat(depthFormat.has_value() ? depthFormat.value() : VK_FORMAT_UNDEFINED)
{
    strncpy(m_name, name, sizeof(m_name) - 1);
    CreateAttachments(numColorTargets, numDepthTargets, colorFormat, depthFormat, clearColorValue, depthStencilClearValue);
}

Loops::Tasking::GraphicsTask::GraphicsTask(const char* name,
    const VkUtils::VulkanContext * const vulkanContext,
    const std::vector<VkImageView>& colorViews,
    const std::vector<VkImageView>& depthViews,
    const VkFormat& colorFormat, const VkFormat& depthFormat) :
    m_vulkanContext(vulkanContext), m_colorAttachmentViews(colorViews),
    m_depthAttachmentViews(depthViews), m_colorFormat(colorFormat),
    m_depthFormat(depthFormat)
{
    strncpy(m_name, name, sizeof(m_name) - 1);
}

Loops::Tasking::GraphicsTask::GraphicsTask(const char* name,
    const VkUtils::VulkanContext * const vulkanContext,
    uint32_t numDepthTargets, const VkFormat& depthFormat,
    const VkClearDepthStencilValue& depthStencilClearValue) : m_vulkanContext(vulkanContext),
    m_depthFormat(depthFormat), m_ownAttachments(true)
{
    strncpy(m_name, name, sizeof(m_name) - 1);
    CreateAttachments(numDepthTargets, depthFormat, depthStencilClearValue);
}

Loops::Tasking::GraphicsTask::GraphicsTask(const char* name,
    const VkUtils::VulkanContext* const vulkanContext,
    const std::vector<VkImageView>& colorViews, const VkFormat& colorFormat):
    m_vulkanContext(vulkanContext), m_colorAttachmentViews(colorViews), m_colorFormat(colorFormat)
{
    strncpy(m_name, name, sizeof(m_name) - 1);
}

Loops::Tasking::GraphicsTask::GraphicsTask(const char* name, const VkUtils::VulkanContext * const vulkanContext) : m_vulkanContext(vulkanContext)
{
    strncpy(m_name, name, sizeof(m_name) - 1);
}

Loops::Tasking::GraphicsTask::~GraphicsTask()
{
    if (m_ownAttachments)
    {
        auto& taskOwnedResource = std::get<Loops::Tasking::TaskOwnedResource>(m_taskResource);
        VkUtils::DestroyRenderTargets(taskOwnedResource.m_colorTargets.data(), taskOwnedResource.m_colorTargets.size(), taskOwnedResource.m_depthTargets.data(),
            taskOwnedResource.m_depthTargets.size(), m_vulkanContext->m_logicalDevice);
    }

    if(m_pipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(m_vulkanContext->m_logicalDevice, m_pipeline, nullptr);
    if(m_vertexShaderModule != VK_NULL_HANDLE)
        vkDestroyShaderModule(m_vulkanContext->m_logicalDevice, m_vertexShaderModule, nullptr);
    if (m_fragmentShaderModule != VK_NULL_HANDLE)
        vkDestroyShaderModule(m_vulkanContext->m_logicalDevice, m_fragmentShaderModule, nullptr);

    if (m_pipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(m_vulkanContext->m_logicalDevice, m_pipelineLayout, nullptr);
    for (auto& layout : m_setLayouts)
        vkDestroyDescriptorSetLayout(m_vulkanContext->m_logicalDevice, layout, nullptr);

    if (m_descriptorPool != VK_NULL_HANDLE)
        vkDestroyDescriptorPool(m_vulkanContext->m_logicalDevice, m_descriptorPool, nullptr);

    if(m_commandPool != VK_NULL_HANDLE)
        vkDestroyCommandPool(m_vulkanContext->m_logicalDevice, m_commandPool, nullptr);
}

const std::vector<VkImageView>& Loops::Tasking::GraphicsTask::GetColorAttachmentViews() const
{
    return m_colorAttachmentViews;
}

const std::vector<VkImageView>& Loops::Tasking::GraphicsTask::GetDepthAttachmentViews() const
{
    ASSERT_MSG(m_depthAttachmentViews.size() > 0, "no depth views");
    return m_depthAttachmentViews;
}

void Loops::Tasking::GraphicsTask::Submit(uint32_t frameInFlight, const VkSemaphore& timelineSem, uint64_t signalValue,
    std::optional<uint64_t> waitValue)
{
    VkSemaphoreSubmitInfo waitInfo{};
    if (waitValue.has_value())
    {
        waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        waitInfo.pNext = nullptr;
        waitInfo.semaphore = timelineSem;
        waitInfo.stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        waitInfo.deviceIndex = 0;
        waitInfo.value = waitValue.value();
    };

    VkSemaphoreSubmitInfo signalInfo
    { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, nullptr, timelineSem, signalValue, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0 };

    VkCommandBufferSubmitInfo bufInfo{};
    bufInfo.commandBuffer = m_commandBuffers[frameInFlight];
    bufInfo.deviceMask = 0;
    bufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;

    VkSubmitInfo2 submitInfo{};
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &bufInfo;
    submitInfo.pSignalSemaphoreInfos = &signalInfo;
    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    if (waitValue.has_value())
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