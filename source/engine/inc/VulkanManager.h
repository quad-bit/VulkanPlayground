#pragma once
#include <memory>
#include <iostream>
#include "ValidationManager.h"
#include "WindowManager.h"
#include "Utils.h"

class VulkanManager
{
private:
    VulkanManager(VulkanManager const&) = delete;
    VulkanManager const& operator= (VulkanManager const&) = delete;

    std::unique_ptr<ValidationManager> m_validationManagerObj;
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
    uint32_t GetSwapchainImageCount() const;
    uint32_t GetFrameInFlightIndex() const;
    uint32_t GetMaxFramesInFlight() const;
    const VkDevice& GetLogicalDevice() const;
    const VkPhysicalDevice& GetPhysicalDevice() const;
    uint32_t GetQueueFamilyIndex() const;
    uint32_t GetActiveSwapchainImageIndex(const VkSemaphore& imageAquiredSignalSemaphore);
    const VkQueue& GetComputeQueue() const;
    const VkQueue& GetGraphicsQueue() const;
    const VkFormat& GetDepthFormat() const;

    const std::vector<VkImageView>& GetDefaultColorImageView() const;
    const std::vector<VkImage>& GetDefaultColorImages() const;
    const std::vector<VkImageView>& GetDefaultDepthImageView() const;
    const std::vector<VkImage>& GetDefaultDepthImages() const;

    void CopyAndPresent(const VkImage& srcImage, const VkSemaphore& semaphore, const VkSemaphore& imageAcquiredSemaphore,
        uint64_t waitValue, uint64_t signalValue);
    bool AreTheQueuesIdle();
};
