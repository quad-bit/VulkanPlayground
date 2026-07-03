#ifndef VULKAN_MANAGER_H
#define VULKAN_MANAGER_H

#include <memory>
#include <iostream>
#include "ValidationManager.h"
#include "WindowManager.h"
#include "Utils.h"

namespace Loops
{
    class VulkanManager
    {
    private:
        VulkanManager(VulkanManager const&) = delete;
        VulkanManager const& operator= (VulkanManager const&) = delete;

        std::unique_ptr<Loops::ValidationManager> m_validationManagerObj;
        std::unique_ptr<WindowManager> m_windowManagerObj;

        VkSurfaceKHR m_surface = VK_NULL_HANDLE;
        VkInstance m_instanceObj = VK_NULL_HANDLE;
        VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
        VkDevice m_logicalDevice = VK_NULL_HANDLE;

        VkQueue m_graphicsQueue = VK_NULL_HANDLE, m_computeQueue = VK_NULL_HANDLE;

        uint32_t m_queueFamilyIndex;

        VkFormat m_depthFormat;
        VkSurfaceFormatKHR m_surfaceFormat;
        VkSurfaceCapabilitiesKHR m_surfaceCapabilities;

        size_t m_surfaceWidth, m_surfaceHeight;
        uint32_t m_swapchainImageCount = 0, m_currentSwpachainIndex = 0;
        uint32_t m_maxFrameInFlight = 0, m_frameInFlightIndex = 0;
        VkSwapchainKHR m_swapchainObj = VK_NULL_HANDLE;
        std::vector<VkImage> m_swapchainImageList;
        std::vector<VkImageView> m_swapChainImageViewList;
        VkClearColorValue m_defaultClearColor{ 0.6033f, 0.6073f, 0.6133f, 1.0f };
        VkClearDepthStencilValue m_defaultDepthClearValue{ 1.0f, 0u };

        void CreateInstance();
        void AcquirePhysicalDevice();
        uint32_t GetQueuesFamilyIndex();
        void CreateLogicalDevice(const uint32_t & queueFamilyIndex);
        void GetMaxUsableVKSampleCount();
        void FindBestDepthFormat();
        void CreateSurface(GLFWwindow* glfwWindow);

        void CreateSwapchain();
        void DestroySwapChain();

        std::vector<VkSemaphore> m_renderingCompletedSignalSemaphore;

        VkCommandPool m_commandPool;
        std::vector<VkCommandBuffer> m_commandBuffers;

        std::vector<VkImage> m_colorAttachments;
        std::vector<VkDeviceMemory> m_colorAttachmentMemory;
        std::vector<VkImageView> m_colorAttachmentViews;

        std::vector<VkImage> m_depthAttachments;
        std::vector<VkDeviceMemory> m_depthAttachmentMemory;
        std::vector<VkImageView> m_depthAttachmentViews;

    public:
        ~VulkanManager();
        VulkanManager(const uint32_t& screenWidth, const uint32_t& screenHeight);

        std::tuple<uint32_t, uint32_t> Init(GLFWwindow* glfwWindow);

        void DeInit();
        void Update(uint32_t currentFrameIndex);
        inline uint32_t GetSwapchainImageCount() const;
        inline uint32_t GetFrameInFlightIndex() const;
        inline uint32_t GetMaxFramesInFlight() const;
        inline const VkDevice& GetLogicalDevice() const;
        inline const VkInstance& GetInstance() const;
        inline const VkPhysicalDevice& GetPhysicalDevice() const;
        inline uint32_t GetQueueFamilyIndex() const;
        inline uint32_t GetActiveSwapchainImageIndex(const VkSemaphore& imageAquiredSignalSemaphore);
        inline const VkQueue& GetComputeQueue() const;
        inline const VkQueue& GetGraphicsQueue() const;
        inline const VkFormat& GetDepthFormat() const;
        inline const VkFormat& GetSurfaceColorFormat() const;
        inline const VkClearColorValue GetDefaultClearColor() const;
        inline const VkClearDepthStencilValue GetDefaultDepthClearValue() const;

        inline const std::vector<VkImageView>& GetDefaultColorImageView() const;
        inline const std::vector<VkImage>& GetDefaultColorImages() const;
        inline const std::vector<VkImageView>& GetDefaultDepthImageView() const;
        inline const std::vector<VkImage>& GetDefaultDepthImages() const;

        void CopyAndPresent(const VkImage& srcImage, const VkSemaphore& semaphore, const VkSemaphore& imageAcquiredSemaphore,
            uint64_t waitValue, uint64_t signalValue);
        bool AreTheQueuesIdle();
    };


    uint32_t Loops::VulkanManager::GetSwapchainImageCount() const
    {
        return m_swapchainImageCount;
    }

    uint32_t Loops::VulkanManager::GetFrameInFlightIndex() const
    {
        return m_frameInFlightIndex;
    }

    uint32_t Loops::VulkanManager::GetMaxFramesInFlight() const
    {
        return m_maxFrameInFlight;
    }

    const VkDevice& Loops::VulkanManager::GetLogicalDevice() const
    {
        return m_logicalDevice;
    }

    const VkPhysicalDevice& Loops::VulkanManager::GetPhysicalDevice() const
    {
        return m_physicalDevice;
    }

    uint32_t Loops::VulkanManager::GetQueueFamilyIndex() const
    {
        return m_queueFamilyIndex;
    }

    uint32_t Loops::VulkanManager::GetActiveSwapchainImageIndex(const VkSemaphore& imageAquiredSignalSemaphore)
    {
        //Get the swapchain image index
        Loops::VkUtils::ErrorCheck(vkAcquireNextImageKHR(m_logicalDevice, m_swapchainObj, UINT64_MAX,
            imageAquiredSignalSemaphore, VK_NULL_HANDLE, &m_currentSwpachainIndex));

        return m_currentSwpachainIndex;
    }

    const VkQueue& Loops::VulkanManager::GetComputeQueue() const
    {
        return m_computeQueue;
    }

    const VkQueue& Loops::VulkanManager::GetGraphicsQueue() const
    {
        return m_graphicsQueue;
    }

    const VkFormat& Loops::VulkanManager::GetDepthFormat() const
    {
        return m_depthFormat;
    }

    const VkFormat& Loops::VulkanManager::GetSurfaceColorFormat() const
    {
        return m_surfaceFormat.format;
    }

    const std::vector<VkImageView>& Loops::VulkanManager::GetDefaultColorImageView() const
    {
        return m_colorAttachmentViews;
    }

    const std::vector<VkImage>& Loops::VulkanManager::GetDefaultColorImages() const
    {
        return m_colorAttachments;
    }

    const std::vector<VkImageView>& Loops::VulkanManager::GetDefaultDepthImageView() const
    {
        return m_depthAttachmentViews;
    }

    const std::vector<VkImage>& Loops::VulkanManager::GetDefaultDepthImages() const
    {
        return m_depthAttachments;
    }

    const VkInstance& Loops::VulkanManager::GetInstance() const
    {
        return m_instanceObj;
    }

    const VkClearColorValue Loops::VulkanManager::GetDefaultClearColor() const
    {
        return m_defaultClearColor;
    }

    const VkClearDepthStencilValue Loops::VulkanManager::GetDefaultDepthClearValue() const
    {
        return m_defaultDepthClearValue;
    }

}
#endif // !VULKAN_MANAGER_H
