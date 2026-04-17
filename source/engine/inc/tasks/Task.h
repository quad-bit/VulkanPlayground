#ifndef TASK_H
#define TASK_H

#include <vector>
#include <array>
#include <vulkan/vulkan.h>
#include <cstring>
#include <memory>
#include <optional>

#include "Utils.h"
//#include "Camera.h"

namespace Common::Tasking
{
    enum class TaskType
    {
        GRAPHICS,
        COMPUTE,
        TRANSFER
    };

    struct GraphicsTaskInfo
    {
        VkDevice m_device;
        VkPhysicalDevice m_physicalDevice;
        Dimension m_renderDimensions;
        VkQueue m_graphicsQueue;
        uint32_t m_queueFamilyIndex;
        uint32_t m_maxFrameInFlights;
    };

    class GraphicsTask
    {
    protected:
        char m_name[32];
        std::vector<VkImage> m_colorAttachments;
        std::vector<VkImageView> m_colorAttachmentViews;
        std::vector<VkImage> m_depthAttachments;
        std::vector<VkImageView> m_depthAttachmentViews;
        VkFormat m_depthFormat, m_colorFormat;

        std::vector<VkImage> m_inputImages;
        std::vector<VkImageView> m_inputViews;

        std::vector<VkBuffer> m_inputBuffers;

        VkCommandPool m_commandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> m_commandBuffers;

        VkShaderModule m_vertexShaderModule = VK_NULL_HANDLE;
        VkShaderModule m_fragmentShaderModule = VK_NULL_HANDLE;
        VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
        VkPipeline m_pipeline = VK_NULL_HANDLE;

        std::vector<VkRenderingAttachmentInfo> m_colorInfoList;
        std::vector<VkRenderingAttachmentInfo> m_depthInfoList;
        std::vector<VkRenderingInfo> m_renderInfoList;

        std::vector<VkDescriptorSetLayout> m_setLayouts;
        VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;

        GraphicsTaskInfo m_info;

        bool m_ownAttachments = false;
        void CreateAttachments();

        void Submit(uint32_t frameInFlight, const VkSemaphore& timelineSem, uint64_t signalValue, std::optional<uint64_t> waitValue);

    public:
        GraphicsTask(const char* name, const GraphicsTaskInfo& info);

        GraphicsTask(const char* name, const GraphicsTaskInfo& info, const std::vector<VkImageView>& colorViews, const std::vector<VkImageView>& depthViews,
            const VkFormat& colorFormat, const VkFormat& depthFormat);

        virtual ~GraphicsTask();

        const std::vector<VkImageView>& GetColorAttachmentViews() const;
        const std::vector<VkImageView>& GetDepthAttachmentViews() const;

    };

    class ComputeTask
    {
    private:
        std::vector<VkImage> m_inputImages;
        std::vector<VkImageView> m_inputViews;

        std::vector<VkBuffer> m_inputBuffers;

        std::vector<VkImage> m_outputImages;
        std::vector<VkBuffer> m_outputBuffers;
    };
}

#endif // !TASK_H
