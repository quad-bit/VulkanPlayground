#include "tasks/Task.h"

void Loops::Tasking::GraphicsTask::CreateAttachments()
{
    assert(0);
}

Loops::Tasking::GraphicsTask::GraphicsTask(const char* name, const GraphicsTaskInfo& info) : m_info(info), m_ownAttachments(true)
{
    strncpy(m_name, name, sizeof(m_name) - 1);
    CreateAttachments();
}

Loops::Tasking::GraphicsTask::GraphicsTask(const char* name, const GraphicsTaskInfo& info, const std::vector<VkImageView>& colorViews,
    const std::vector<VkImageView>& depthViews, const VkFormat& colorFormat, const VkFormat& depthFormat) :
    m_info(info), m_colorAttachmentViews(colorViews), m_depthAttachmentViews(depthViews), m_colorFormat(colorFormat), m_depthFormat(depthFormat)
{
    strncpy(m_name, name, sizeof(m_name) - 1);
}

Loops::Tasking::GraphicsTask::~GraphicsTask()
{
    if (m_ownAttachments)
    {
        assert(0);
    }

    vkDestroyPipeline(m_info.m_device, m_pipeline, nullptr);
    vkDestroyShaderModule(m_info.m_device, m_vertexShaderModule, nullptr);
    vkDestroyShaderModule(m_info.m_device, m_fragmentShaderModule, nullptr);

    vkDestroyPipelineLayout(m_info.m_device, m_pipelineLayout, nullptr);
    for (auto& layout : m_setLayouts)
        vkDestroyDescriptorSetLayout(m_info.m_device, layout, nullptr);

    vkDestroyDescriptorPool(m_info.m_device, m_descriptorPool, nullptr);

    vkDestroyCommandPool(m_info.m_device, m_commandPool, nullptr);
}

const std::vector<VkImageView>& Loops::Tasking::GraphicsTask::GetColorAttachmentViews() const
{
    return m_colorAttachmentViews;
}

const std::vector<VkImageView>& Loops::Tasking::GraphicsTask::GetDepthAttachmentViews() const
{
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
        Loops::VkUtils::ErrorCheck(vkQueueSubmit2(m_info.m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
    }
}