#pragma once
#include "Utils.h"
#include "Camera.h"
#include "RenderData.h"
#include "SceneManager.h"
#include <optional>
#include <memory>

namespace Common
{
    class WireFrameTask
    {
    private:
        const VkQueue& m_graphicsQueue;
        const VkDevice& m_device;
        const VkPhysicalDevice& m_physicalDevice;
        const uint32_t cm_maxEntities;

        VkCommandPool m_commandPool;
        std::vector<VkCommandBuffer> m_commandBuffers;
        VkPipelineLayout m_pipelineLayout;
        VkShaderModule m_shaderModule;

        VkShaderModule m_vertexShaderModule = VK_NULL_HANDLE;
        VkShaderModule m_fragmentShaderModule = VK_NULL_HANDLE;

        std::vector<VkImageView> m_colorAttachmentViews, m_depthAttachmentViews;

        VkSampler m_immutableSampler;

        const uint16_t CAMERA_SET = 0;
        const uint16_t TRANSFORM_SET = 1;
        std::vector<VkDescriptorSetLayout> m_setLayouts;

        std::vector<VkDescriptorSet> m_transformSets;
        VkBuffer m_transformBuffer;
        VkDeviceMemory m_transformUniformMemory;

        std::vector<VkDescriptorSet> m_viewSet;
        VkBuffer m_cameraUniforms;
        VkDeviceMemory m_cameraUniformMemory;
        std::shared_ptr<Common::Camera> m_pCamera;

        VkDescriptorPool m_descriptorPool;

        std::vector<VkRenderingAttachmentInfo> colorInfoList;
        std::vector<VkRenderingAttachmentInfo> depthInfoList;
        std::vector<VkRenderingInfo> m_renderInfoList;

        VkPipeline m_pipeline;
        uint32_t m_screenWidth;
        uint32_t m_screenHeight;
        uint32_t m_maxFrameInFlights;
        uint32_t m_numIndicies;

        void BuildCommandBuffers(const uint32_t& frameInFlight, const Common::RenderData& renderData, const Common::SceneManager& sceneManager);

    public:

        WireFrameTask(const VkDevice& device, const VkPhysicalDevice& physicalDevice, const VkQueue& graphicsQueue,
            uint32_t queueFamilyIndex, uint32_t maxFrameInFlight, uint32_t screenWidth, uint32_t screenHeight, const VkFormat& depthFormat,
            const std::vector<VkImageView>& colorViews, const std::vector<VkImageView>& depthViews, const uint32_t& maxEntities, std::shared_ptr<Camera> pCamera = nullptr);
        ~WireFrameTask();

        //Create quad draw specific resources
        void Update(const uint64_t& frameIndex, const uint32_t& frameInFlight,
            const VkSemaphore& timelineSem, uint64_t signalValue, std::optional<uint64_t> waitValue,
            const Common::RenderData& renderData, const Common::SceneManager& sceneManager);
        //const std::vector<VkImage>& GetColorAttachments() const;
        const std::vector<VkImageView>& GetColorAttachmentViews() const;
    };
}