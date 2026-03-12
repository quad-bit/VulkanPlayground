#include "..\inc\VulkanManager.h"
#include <vector>
#include <array>
#include "Utils.h"

namespace
{
    // For a minimum of two queues compute and graphics
    float queuePriority[2]{ 1.0f, 1.0f };

    std::vector<VkDeviceQueueCreateInfo> FindQueue(const uint32_t & queueFamilyIndex)
    {
        constexpr uint32_t minGraphicQueueRequired = 1, minCopmuteQueueRequired = 1;

        std::vector<VkDeviceQueueCreateInfo> creatInfoList;

        VkDeviceQueueCreateInfo info = {};
        info.flags = 0;
        info.pNext = nullptr;
        info.pQueuePriorities = queuePriority;
        info.queueCount = minGraphicQueueRequired + minCopmuteQueueRequired;
        info.queueFamilyIndex = queueFamilyIndex;
        info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

        creatInfoList.push_back(info);

        return creatInfoList;
    }

    void MakeSwapchainImagesPresentable(const VkDevice& device, std::vector<VkImage>& imageList, const VkQueue& queue, uint32_t queueFamilyIndex)
    {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandPoolCreateInfo info{};
        info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        info.pNext = nullptr;
        info.queueFamilyIndex = queueFamilyIndex;
        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

        ErrorCheck(vkCreateCommandPool(device, &info, nullptr, &pool));

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.commandBufferCount = 1;
        allocInfo.commandPool = pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.pNext = nullptr;
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

        VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
        ErrorCheck(vkAllocateCommandBuffers(device, &allocInfo, &cmdBuffer));

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;
        beginInfo.pNext = nullptr;
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkBeginCommandBuffer(cmdBuffer, &beginInfo);
        std::vector<VkImageMemoryBarrier2> list;
        for (auto& image : imageList)
        {
            VkImageMemoryBarrier2 imgBarrier{};
            imgBarrier.dstAccessMask = 0;
            imgBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imgBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            imgBarrier.image = image;
            imgBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            imgBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imgBarrier.pNext = nullptr;
            imgBarrier.srcAccessMask = 0;
            imgBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            imgBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
            imgBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            imgBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            imgBarrier.subresourceRange.baseArrayLayer = 0;
            imgBarrier.subresourceRange.baseMipLevel = 0;
            imgBarrier.subresourceRange.layerCount = 1;
            imgBarrier.subresourceRange.levelCount = 1;
            list.push_back(imgBarrier);
        }

        VkDependencyInfo dependencyInfo{};
        dependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
        dependencyInfo.imageMemoryBarrierCount = list.size();
        dependencyInfo.pImageMemoryBarriers = list.data();
        dependencyInfo.pNext = nullptr;
        dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

        vkCmdPipelineBarrier2(cmdBuffer, &dependencyInfo);
        vkEndCommandBuffer(cmdBuffer);

        VkFence fence = VK_NULL_HANDLE;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.pNext = nullptr;
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        ErrorCheck(vkCreateFence(device, &fenceInfo, nullptr, &fence));

        VkSubmitInfo submitInfo{};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;
        submitInfo.pNext = nullptr;
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        ErrorCheck(vkQueueSubmit(queue, 1, &submitInfo, fence));

        ErrorCheck(vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX));

        vkDestroyFence(device, fence, nullptr);
        vkDestroyCommandPool(device, pool, nullptr);
    }
}

void VulkanManager::CreateInstance()
{
    VkApplicationInfo appInfo{};
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pApplicationName = "VulkanCompute";
    appInfo.pEngineName = "None";
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfoObj{};
    createInfoObj.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfoObj.pApplicationInfo = &appInfo;
    createInfoObj.enabledExtensionCount = (uint32_t)m_validationManagerObj->instanceExtensionNameList.size();
    createInfoObj.enabledLayerCount = (uint32_t)m_validationManagerObj->instanceLayerNameList.size();
    createInfoObj.pNext = &(m_validationManagerObj->dbg_messenger_create_info);
    createInfoObj.ppEnabledExtensionNames = m_validationManagerObj->instanceExtensionNameList.data();
    createInfoObj.ppEnabledLayerNames = m_validationManagerObj->instanceLayerNameList.data();

    ErrorCheck(vkCreateInstance(&createInfoObj, nullptr, &m_instanceObj));
}

void VulkanManager::CreateLogicalDevice(const uint32_t & queueFamilyIndex)
{
    VkPhysicalDeviceTimelineSemaphoreFeatures timelineFeatures{};
    timelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    timelineFeatures.timelineSemaphore = VK_TRUE;

    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_feature{};
    dynamic_rendering_feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamic_rendering_feature.dynamicRendering = VK_TRUE;
    dynamic_rendering_feature.pNext = &timelineFeatures;

    VkPhysicalDeviceSynchronization2Features sync2 = {};
    sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
    sync2.synchronization2 = VK_TRUE;
    sync2.pNext = &dynamic_rendering_feature;

    VkPhysicalDeviceFeatures2 physicalFeatures2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    physicalFeatures2.pNext = &sync2;
    vkGetPhysicalDeviceFeatures2(m_physicalDevice, &physicalFeatures2);

    std::vector<VkDeviceQueueCreateInfo> deviceQueueCreateInfoList = FindQueue(queueFamilyIndex);

    VkDeviceCreateInfo vkDeviceCreateInfoObj{};
    vkDeviceCreateInfoObj.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    vkDeviceCreateInfoObj.queueCreateInfoCount = (uint32_t)deviceQueueCreateInfoList.size();
    vkDeviceCreateInfoObj.pQueueCreateInfos = deviceQueueCreateInfoList.data();
    vkDeviceCreateInfoObj.enabledExtensionCount = (uint32_t)m_validationManagerObj->deviceExtensionNameList.size();
    vkDeviceCreateInfoObj.enabledLayerCount = 0;
    vkDeviceCreateInfoObj.ppEnabledExtensionNames = m_validationManagerObj->deviceExtensionNameList.data();
    vkDeviceCreateInfoObj.ppEnabledLayerNames = nullptr;
    vkDeviceCreateInfoObj.pNext = &physicalFeatures2;

    ErrorCheck(vkCreateDevice(m_physicalDevice, &vkDeviceCreateInfoObj, nullptr, &m_logicalDevice));
}

void VulkanManager::AcquirePhysicalDevice()
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(m_instanceObj, &count, nullptr);
    std::vector<VkPhysicalDevice> deviceList(count);
    vkEnumeratePhysicalDevices(m_instanceObj, &count, deviceList.data());

    VkPhysicalDevice discreteGpu = VK_NULL_HANDLE;
    VkPhysicalDevice integratedGpu = VK_NULL_HANDLE;
    for (auto dev : deviceList)
    {
        VkPhysicalDeviceProperties deviceProp = {};
        vkGetPhysicalDeviceProperties(dev, &deviceProp);

        if (deviceProp.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) // deviceProp.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ||
        {
            discreteGpu = dev;
            break;
        }

        if (deviceProp.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
        {
            integratedGpu = dev;
        }
    }

    if (discreteGpu != VK_NULL_HANDLE)
    {
        m_physicalDevice = discreteGpu;
    }
    else if (integratedGpu != VK_NULL_HANDLE)
    {
        m_physicalDevice = integratedGpu;
    }

    if (m_physicalDevice == VK_NULL_HANDLE)
    {
        assert(0);
        std::exit(-1);
    }
}

uint32_t VulkanManager::GetQueuesFamilyIndex()
{
    uint32_t graphicsReq = VK_QUEUE_GRAPHICS_BIT;
    uint32_t computeReq = VK_QUEUE_COMPUTE_BIT;
    uint32_t transferReq = VK_QUEUE_TRANSFER_BIT;

    uint32_t graphicsQueueFamilyIndex, computeQueueFamilyIndex;

    uint32_t qFamilyCount = 0;
    std::vector<VkQueueFamilyProperties> propertyList;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &qFamilyCount, nullptr);
    propertyList.resize(qFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &qFamilyCount, propertyList.data());

    uint16_t count = 0;
    bool sameFamily = false;
    for (uint32_t j = 0; j < qFamilyCount; j++)
    {
        count = 0;

        if (propertyList[j].queueFlags & graphicsReq == graphicsReq)
        {
            graphicsQueueFamilyIndex = j;
            count++;
        }

        if (propertyList[j].queueFlags & computeReq == computeReq)
        {
            computeQueueFamilyIndex = j;
            count++;
        }
        if (count == 2)
        {
            sameFamily = true;
            break;
        }
    }

    assert(sameFamily == true);
    return graphicsQueueFamilyIndex;
}

void VulkanManager::GetMaxUsableVKSampleCount()
{
}

void VulkanManager::FindBestDepthFormat()
{
    VkFormat formatList[5]{
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_D24_UNORM_S8_UINT,
    VK_FORMAT_D16_UNORM_S8_UINT,
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_D16_UNORM };

    VkFormatProperties props = {};

    for (uint32_t i = 0; i < 5; i++)
    {
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, formatList[i], &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            m_depthFormat = formatList[i];
            return;
        }
    }
    assert(0);
}

VulkanManager::~VulkanManager()
{
}

VulkanManager::VulkanManager(const uint32_t& screenWidth, const uint32_t& screenHeight) : m_surfaceWidth(screenWidth), m_surfaceHeight(screenHeight)
{
    m_validationManagerObj = std::make_unique<ValidationManager>();
}

std::tuple<uint32_t, uint32_t> VulkanManager::Init(GLFWwindow* glfwWindow)
{
    CreateInstance();
    AcquirePhysicalDevice();
    m_validationManagerObj->InitDebug(&m_instanceObj, nullptr);
    m_queueFamilyIndex = GetQueuesFamilyIndex();
    CreateLogicalDevice(m_queueFamilyIndex);

    vkGetDeviceQueue(m_logicalDevice, m_queueFamilyIndex, 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_logicalDevice, m_queueFamilyIndex, 1, &m_computeQueue);

    GetMaxUsableVKSampleCount();
    FindBestDepthFormat();
    CreateSurface(glfwWindow);
    CreateSwapchain();

    MakeSwapchainImagesPresentable(m_logicalDevice, m_swapchainImageList, m_graphicsQueue, m_queueFamilyIndex);

    {
        for (uint32_t i = 0; i < m_swapchainImageCount; i++)
        {
            VkSemaphoreCreateInfo semInfo{};
            semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            VkSemaphore semaphore = VK_NULL_HANDLE;
            ErrorCheck(vkCreateSemaphore(m_logicalDevice, &semInfo, nullptr, &semaphore));
            m_renderingCompletedSignalSemaphore.push_back(semaphore);
        }
    }

    {
        VkCommandPoolCreateInfo createInfo{};
        createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        createInfo.queueFamilyIndex = m_queueFamilyIndex;
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

        ErrorCheck(vkCreateCommandPool(m_logicalDevice, &createInfo, nullptr, &m_commandPool));

        m_commandBuffers.resize(m_maxFrameInFlight);
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.commandBufferCount = m_maxFrameInFlight;
        alloc_info.commandPool = m_commandPool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

        ErrorCheck(vkAllocateCommandBuffers(m_logicalDevice, &alloc_info, &m_commandBuffers[0]));
    }

    {
        // Render pass attachments
        m_colorAttachmentViews.resize(m_maxFrameInFlight);
        m_colorAttachments.resize(m_maxFrameInFlight);
        m_colorAttachmentMemory.resize(m_maxFrameInFlight);
        for (int i = 0; i < m_maxFrameInFlight; ++i)
        {
            std::tie(m_colorAttachments[i], m_colorAttachmentMemory[i]) = CreateImage(m_logicalDevice, m_physicalDevice, m_surfaceWidth, m_surfaceHeight, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);

            VkImageViewCreateInfo createInfo{};
            createInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY };
            createInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
            createInfo.image = m_colorAttachments[i];
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.layerCount = 1;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ErrorCheck(vkCreateImageView(m_logicalDevice, &createInfo, nullptr, &m_colorAttachmentViews[i]));
        }

        //Depth
        m_depthAttachmentViews.resize(m_maxFrameInFlight);
        m_depthAttachments.resize(m_maxFrameInFlight);
        m_depthAttachmentMemory.resize(m_maxFrameInFlight);
        for (int i = 0; i < m_maxFrameInFlight; ++i)
        {
            std::tie(m_depthAttachments[i], m_depthAttachmentMemory[i]) = CreateImage(m_logicalDevice, m_physicalDevice, m_surfaceWidth, m_surfaceHeight, m_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
            VkImageViewCreateInfo createInfo{};
            createInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY };
            createInfo.format = m_depthFormat;
            createInfo.image = m_depthAttachments[i];
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.layerCount = 1;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ErrorCheck(vkCreateImageView(m_logicalDevice, &createInfo, nullptr, &m_depthAttachmentViews[i]));
        }

        ChangeImageLayout(m_logicalDevice, m_colorAttachments, m_graphicsQueue, m_queueFamilyIndex, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        ChangeImageLayout(m_logicalDevice, m_depthAttachments, m_graphicsQueue, m_queueFamilyIndex, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }

    return { m_surfaceWidth, m_surfaceHeight };
}

void VulkanManager::DeInit()
{
    vkQueueWaitIdle(m_computeQueue);
    vkQueueWaitIdle(m_graphicsQueue);

//    vkDestroySemaphore(m_logicalDevice, m_cpuWaitSemaphore, nullptr);

    for (uint32_t i = 0; i < m_maxFrameInFlight; i++)
    {
        vkDestroyImageView(m_logicalDevice, m_colorAttachmentViews[i], nullptr);
        vkFreeMemory(m_logicalDevice, m_colorAttachmentMemory[i], nullptr);
        vkDestroyImage(m_logicalDevice, m_colorAttachments[i], nullptr);

        vkDestroyImageView(m_logicalDevice, m_depthAttachmentViews[i], nullptr);
        vkFreeMemory(m_logicalDevice, m_depthAttachmentMemory[i], nullptr);
        vkDestroyImage(m_logicalDevice, m_depthAttachments[i], nullptr);
    }

    for (uint32_t i = 0; i < m_swapchainImageCount; i++)
    {
        vkDestroySemaphore(m_logicalDevice, m_renderingCompletedSignalSemaphore[i], nullptr);
    }

    vkDestroyCommandPool(m_logicalDevice, m_commandPool, nullptr);

    DestroySwapChain();
    vkDestroySurfaceKHR(m_instanceObj, m_surface, nullptr);
    vkDestroyDevice(m_logicalDevice, nullptr);
    m_validationManagerObj->DeinitDebug();

    vkDestroyInstance(m_instanceObj, nullptr);
}

void VulkanManager::Update(uint32_t currentFrameIndex)
{

}

uint32_t VulkanManager::GetSwapchainImageCount() const
{
    return m_swapchainImageCount;
}

uint32_t VulkanManager::GetFrameInFlightIndex() const
{
    return m_frameInFlightIndex;
}

uint32_t VulkanManager::GetMaxFramesInFlight() const
{
    return m_maxFrameInFlight;
}

const VkDevice& VulkanManager::GetLogicalDevice() const
{
    return m_logicalDevice;
}

const VkPhysicalDevice & VulkanManager::GetPhysicalDevice() const
{
    return m_physicalDevice;
}

uint32_t VulkanManager::GetQueueFamilyIndex() const
{
    return m_queueFamilyIndex;
}

uint32_t VulkanManager::GetActiveSwapchainImageIndex(const VkSemaphore& imageAquiredSignalSemaphore)
{
    //Get the swapchain image index
    ErrorCheck(vkAcquireNextImageKHR(m_logicalDevice, m_swapchainObj, UINT64_MAX,
        imageAquiredSignalSemaphore, VK_NULL_HANDLE, &m_currentSwpachainIndex));

    return m_currentSwpachainIndex;
}

const VkQueue & VulkanManager::GetComputeQueue() const
{
    return m_computeQueue;
}

const VkQueue & VulkanManager::GetGraphicsQueue() const
{
    return m_graphicsQueue;
}

const VkFormat & VulkanManager::GetDepthFormat() const
{
    return m_depthFormat;
}

const std::vector<VkImageView>& VulkanManager::GetDefaultColorImageView() const
{
    return m_colorAttachmentViews;
}

const std::vector<VkImage>& VulkanManager::GetDefaultColorImages() const
{
    return m_colorAttachments;
}

const std::vector<VkImageView>& VulkanManager::GetDefaultDepthImageView() const
{
    return m_depthAttachmentViews;
}

const std::vector<VkImage>& VulkanManager::GetDefaultDepthImages() const
{
    return m_depthAttachments;
}

void VulkanManager::CopyAndPresent(const VkImage & srcImage, const VkSemaphore& semaphore, const VkSemaphore& imageAcquiredSemaphore,
    uint64_t waitValue, uint64_t signalValue)
{
    // Change layout to tranfer dst, then copy and change it to present layout
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    ErrorCheck(vkBeginCommandBuffer(m_commandBuffers[m_frameInFlightIndex], &beginInfo));

    std::vector<VkImageMemoryBarrier> image_barrier;
    //if (srcImageLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        VkImageMemoryBarrier srcBarrier{};
        srcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        srcBarrier.srcAccessMask = 0;
        srcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        srcBarrier.image = srcImage;
        srcBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        srcBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        srcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        image_barrier.push_back(srcBarrier);
    }

    VkImageMemoryBarrier swapBarrier{};
    swapBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    swapBarrier.srcAccessMask = 0;
    swapBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    swapBarrier.image = m_swapchainImageList[m_currentSwpachainIndex];
    swapBarrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    swapBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    swapBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    image_barrier.push_back(swapBarrier);

    // The semaphore takes care of srcStageMask.
    vkCmdPipelineBarrier(m_commandBuffers[m_frameInFlightIndex],
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, image_barrier.size(), image_barrier.data());

    VkImageCopy region{};
    region.dstOffset = { 0,0,0 };
    region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.extent = { (uint32_t)m_surfaceWidth, (uint32_t)m_surfaceHeight, 1 };
    region.srcOffset = { 0,0,0 };
    region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };

    vkCmdCopyImage(m_commandBuffers[m_frameInFlightIndex],
        srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        m_swapchainImageList[m_currentSwpachainIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region);

    // Make it presentable

    std::array< VkImageMemoryBarrier, 2> barrier2{};
    barrier2[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier2[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier2[0].dstAccessMask = 0;
    barrier2[0].image = m_swapchainImageList[m_currentSwpachainIndex];
    barrier2[0].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    barrier2[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier2[0].newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    barrier2[1].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier2[1].srcAccessMask = 0; // not sure lets see
    barrier2[1].dstAccessMask = 0; // not sure lets see
    barrier2[1].image = srcImage;
    barrier2[1].subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    barrier2[1].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier2[1].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // The semaphore takes care of srcStageMask.
    vkCmdPipelineBarrier(m_commandBuffers[m_frameInFlightIndex],
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr, barrier2.size(), barrier2.data());

    ErrorCheck(vkEndCommandBuffer(m_commandBuffers[m_frameInFlightIndex]));

    VkSemaphoreSubmitInfo signalInfo[2]{
        {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, nullptr, m_renderingCompletedSignalSemaphore[m_currentSwpachainIndex], 0, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0},
        {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, nullptr, semaphore, signalValue, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0}
    };
    
    VkSemaphoreSubmitInfo waitInfo[2]{
        {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, nullptr, semaphore, waitValue, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0},
        {VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, nullptr, imageAcquiredSemaphore, 0, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0}
    };
    
    VkCommandBufferSubmitInfo cmdInfo{};
    cmdInfo.commandBuffer = m_commandBuffers[m_frameInFlightIndex];
    cmdInfo.deviceMask = 0;
    cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;

    VkSubmitInfo2 submitInfo{};
    submitInfo.commandBufferInfoCount = 1;
    submitInfo.pCommandBufferInfos = &cmdInfo;
    submitInfo.pSignalSemaphoreInfos = &signalInfo[0];
    submitInfo.pWaitSemaphoreInfos = &waitInfo[0];
    submitInfo.signalSemaphoreInfoCount = 2;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submitInfo.waitSemaphoreInfoCount = 2;
    ErrorCheck(vkQueueSubmit2(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));

    VkPresentInfoKHR presentInfo{};
    presentInfo.pImageIndices = &m_currentSwpachainIndex;
    presentInfo.pSwapchains = &m_swapchainObj;
    presentInfo.pWaitSemaphores = &m_renderingCompletedSignalSemaphore[m_currentSwpachainIndex];
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.waitSemaphoreCount = 1;

    ErrorCheck(vkQueuePresentKHR(m_graphicsQueue, &presentInfo));

    m_frameInFlightIndex = (m_frameInFlightIndex + 1) % m_maxFrameInFlight;
}

bool VulkanManager::AreTheQueuesIdle()
{
    vkQueueWaitIdle(m_computeQueue);
    vkQueueWaitIdle(m_graphicsQueue);

    return true;
}

void VulkanManager::CreateSurface(GLFWwindow * glfwWindow)
{
#if defined(GLFW_ENABLED)
    if (VK_SUCCESS != glfwCreateWindowSurface(m_instanceObj, glfwWindow, nullptr, &m_surface))
    {
        glfwTerminate();
        assert(0);
        std::exit(-1);
    }
#else
    ASSERT_MSG(0, "GLFW not getting used, need to implement OS specific implementation");
    std::exit(-1);
#endif

    VkBool32 WSI_supported = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, m_queueFamilyIndex, m_surface, &WSI_supported);
    if (!WSI_supported)
    {
        assert(0);
        std::exit(-1);
    }

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &m_surfaceCapabilities);

    if (m_surfaceCapabilities.currentExtent.width < UINT32_MAX)
    {
        m_surfaceWidth = m_surfaceCapabilities.currentExtent.width;
        m_surfaceHeight = m_surfaceCapabilities.currentExtent.height;
    }

    {
        uint32_t format_count = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &format_count, nullptr);
        if (format_count == 0)
        {
            assert(0);
            std::exit(-1);
        }
        std::vector<VkSurfaceFormatKHR> formats(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &format_count, formats.data());
        if (formats[0].format == VK_FORMAT_UNDEFINED)
        {
            m_surfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
            m_surfaceFormat.colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;
        }
        else
        {
            m_surfaceFormat = formats[0];
        }
    }
}

void VulkanManager::CreateSwapchain()
{
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &m_surfaceCapabilities);

    if (m_surfaceCapabilities.maxImageCount > 0)
        if (m_swapchainImageCount > m_surfaceCapabilities.maxImageCount)
            m_swapchainImageCount = m_surfaceCapabilities.maxImageCount;

    if (m_swapchainImageCount < m_surfaceCapabilities.minImageCount + 1)
        m_swapchainImageCount = m_surfaceCapabilities.minImageCount + 1;

    auto presentMode = VkPresentModeKHR::VK_PRESENT_MODE_FIFO_KHR;
    {
        uint32_t count = 0;
        ErrorCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &count, nullptr));
        std::vector<VkPresentModeKHR> presentModeList(count);
        ErrorCheck(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &count, presentModeList.data()));

        for (VkPresentModeKHR obj : presentModeList)
        {
            if (obj == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                presentMode = obj;
                m_swapchainImageCount = 3;
                break;
            }
        }
    }

    m_maxFrameInFlight = m_swapchainImageCount - 1;

    VkSwapchainCreateInfoKHR swapChainCreateInfo{};
    swapChainCreateInfo.clipped = VK_TRUE; // dont render parts of swapchain image that are out of the frustrum
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreateInfo.imageArrayLayers = 1; // 2 meant for sterescopic rendering
    swapChainCreateInfo.imageColorSpace = m_surfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent.height = m_surfaceHeight;
    swapChainCreateInfo.imageExtent.width = m_surfaceWidth;
    swapChainCreateInfo.imageFormat = m_surfaceFormat.format;
    swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    swapChainCreateInfo.minImageCount = m_swapchainImageCount;
    swapChainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapChainCreateInfo.presentMode = presentMode;
    swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE; // useful when resizing the window
    swapChainCreateInfo.queueFamilyIndexCount = 0; // as its not shared between multiple queues
    swapChainCreateInfo.pQueueFamilyIndices = nullptr;
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.surface = m_surface;

    ErrorCheck(vkCreateSwapchainKHR(m_logicalDevice, &swapChainCreateInfo, nullptr, &m_swapchainObj));

    // swapchain images
    uint32_t count = 0;
    ErrorCheck(vkGetSwapchainImagesKHR(m_logicalDevice, m_swapchainObj, &count, nullptr));
    m_swapchainImageList.resize(count);
    ErrorCheck(vkGetSwapchainImagesKHR(m_logicalDevice, m_swapchainObj, &count, m_swapchainImageList.data()));
    assert(m_swapchainImageCount == (uint32_t)m_swapchainImageList.size());

    // swapchain image views
    m_swapChainImageViewList.resize(m_swapchainImageCount);
    for (uint32_t i = 0; i < m_swapchainImageCount; i++)
    {
        VkImageViewCreateInfo createInfo{};
        createInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY };
        createInfo.format = m_surfaceFormat.format;
        createInfo.image = m_swapchainImageList[i];
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.layerCount = 1;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

        ErrorCheck(vkCreateImageView(m_logicalDevice, &createInfo, nullptr, &m_swapChainImageViewList[i]));
    }
}

void VulkanManager::DestroySwapChain()
{
    for (auto& view : m_swapChainImageViewList)
    {
        vkDestroyImageView(m_logicalDevice, view, nullptr);
    }
    vkDestroySwapchainKHR(m_logicalDevice, m_swapchainObj, nullptr);
}
