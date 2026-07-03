#ifndef IMGUI_SYSTEM_H
#define IMGUI_SYSTEM_H

#include <functional>
#include <imgui.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "VulkanManager.h"

// Clang/GCC warnings with -Weverything
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast" // warning: use of old-style cast
#endif

#if defined(IMGUI_IMPL_VULKAN_NO_PROTOTYPES) && !defined(VK_NO_PROTOTYPES)
#define VK_NO_PROTOTYPES
#endif
#if defined(VK_USE_PLATFORM_WIN32_KHR) && !defined(NOMINMAX)
#define NOMINMAX
#endif

// Vulkan includes
#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
#ifdef IMGUI_IMPL_VULKAN_VOLK_FILENAME
#include IMGUI_IMPL_VULKAN_VOLK_FILENAME
#else
#include <volk.h>
#endif
#else
#include <vulkan/vulkan.h>
#endif
#if defined(VK_VERSION_1_3) || defined(VK_KHR_dynamic_rendering)
#define IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
#endif

// Backend uses a small number of descriptors per font atlas + as many as additional calls done to ImGui_ImplVulkan_AddTexture().
#define IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE   (8)     // Minimum per atlas
#define IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE         (2)     // Minimum for linear + nearest

namespace Loops
{

    struct ImGuiInitInfo
    {
        uint32_t                        m_apiVersion = VK_API_VERSION_1_3;                 // Fill with API version of Instance, e.g. VK_API_VERSION_1_3 or your value of VkApplicationInfo::apiVersion. May be lower than header version (VK_HEADER_VERSION_COMPLETE)
        VkInstance                      m_instance = VK_NULL_HANDLE;
        VkPhysicalDevice                m_physicalDevice = VK_NULL_HANDLE;
        VkDevice                        m_device = VK_NULL_HANDLE;
        uint32_t                        m_queueFamily = 0;
        VkQueue                         m_queue = VK_NULL_HANDLE;
        VkDescriptorPool                m_descriptorPool = VK_NULL_HANDLE;             // See requirements in note above; ignored if using DescriptorPoolSize > 0
        uint32_t                        m_descriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE;         // Optional: set to create internal ImageView descriptor pool automatically instead of using DescriptorPool.
        uint32_t                        m_minImageCount = 2;              // >= 2
        uint32_t                        m_imageCount = 2;                 // >= MinImageCount
        VkPipelineCache                 m_pipelineCache = VK_NULL_HANDLE;              // Optional

        // Pipeline
        VkPipelineRenderingCreateInfoKHR m_pipelineRenderingCreateInfo;           // Infos for Main Viewport (created by app/user)
        //VkRenderPass                  RenderPass;                 // --> Since 2025/09/26: set 'PipelineInfoMain.RenderPass' instead
        //uint32_t                      Subpass;                    // --> Since 2025/09/26: set 'PipelineInfoMain.Subpass' instead
        //VkSampleCountFlagBits         MSAASamples;                // --> Since 2025/09/26: set 'PipelineInfoMain.MSAASamples' instead
        //VkPipelineRenderingCreateInfoKHR PipelineRenderingCreateInfo; // Since 2025/09/26: set 'PipelineInfoMain.PipelineRenderingCreateInfo' instead

        // (Optional) Dynamic Rendering
        // Need to explicitly enable VK_KHR_dynamic_rendering extension to use this, even for Vulkan 1.3 + setup PipelineInfoMain.PipelineRenderingCreateInfo.
        bool                            m_useDynamicRendering = true;

        // (Optional) Allocation, Debugging
        const VkAllocationCallbacks*    m_allocator = nullptr;
        VkDeviceSize                    m_minAllocationSize = 1024 * 1024;          // Minimum allocation size. Set to 1024*1024 to satisfy zealous best practices validation layer and waste a little memory.

        // (Optional) Customize default vertex/fragment shaders.
        // - if .sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO we use specified structs, otherwise we use defaults.
        // - Shader inputs/outputs need to match ours. Code/data pointed to by the structure needs to survive for whole during of backend usage.
        VkShaderModuleCreateInfo        m_customShaderVertCreateInfo;
        VkShaderModuleCreateInfo        m_customShaderFragCreateInfo;
    };

    struct ImGuiRenderState
    {
        VkCommandBuffer     m_commandBuffer = VK_NULL_HANDLE;
        VkPipeline          m_pipeline = VK_NULL_HANDLE;
        VkPipelineLayout    m_pipelineLayout = VK_NULL_HANDLE;
    };

    class ImguiSystem
    {
    private:
        std::vector<std::function<void()>> m_guiDrawPersistentList;
        std::vector<std::function<void(uint32_t)>> m_guiDrawOnFramePersistentList;

        VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
        VkCommandPool m_commandPool;
        std::vector<VkCommandBuffer> m_commandBuffers;
        uint32_t m_frameBufferWidth = 0;
        uint32_t m_framebufferHeight = 0;
        uint32_t m_maxFrameInFlights = 0;
        const std::vector<VkImageView>& m_colorAttachmentViews;
        std::vector<VkRenderingAttachmentInfo> m_colorList;
        std::vector<VkRenderingInfo> m_renderingInfoList;
        VkFormat m_surfaceColorFormat;

        GLFWwindow*             m_glfwWindow = nullptr;
        GLFWwindowfocusfun      m_userCallbackWindowFocus;
        GLFWcursorposfun        m_userCallbackCursorPos;
        GLFWcursorenterfun      m_userCallbackCursorEnter;
        GLFWmousebuttonfun      m_userCallbackMousebutton;
        GLFWscrollfun           m_userCallbackScroll;
        GLFWkeyfun              m_userCallbackKey;
        GLFWcharfun             m_userCallbackChar;

        static GLFWwindow*      m_mouseWindow;
        static ImVec2           m_lastValidMousePos;
        bool                    m_clearRenderTarget = false;
        bool                    m_renderingInfoCreated = false;

    public:

        /*ImguiSystem(GLFWwindow* window, const Loops::VulkanManager* vulkanManager, const VkDevice& device, const VkPhysicalDevice& physicalDevice,
            const VkQueue& graphicsQueue, uint32_t graphicsQueueFamily, uint8_t frameInFlights,
            uint32_t frameBufferWidth, uint32_t framebufferHeight, VkFormat depthFormat, VkFormat colorFormat,
            const std::vector<VkImageView>& colorViews
        );*/

        ImguiSystem(GLFWwindow* window, const Loops::VulkanManager* vulkanManager, const VkDevice& device, const VkPhysicalDevice& physicalDevice,
            const VkQueue& graphicsQueue, uint32_t graphicsQueueFamily, uint8_t frameInFlights,
            uint32_t frameBufferWidth, uint32_t framebufferHeight,VkFormat colorFormat, const std::vector<VkImageView>& colorViews
        );


        void CreateRenderingInfo();

        void CreateRenderingInfo(const VkClearColorValue& clearColor);

        ~ImguiSystem();

        // Follow "Getting Started" link and check examples/ folder to learn about using backends!
        IMGUI_IMPL_API bool ImGuiInit(ImGuiInitInfo* info);
        IMGUI_IMPL_API void ImGuiShutdown();
        IMGUI_IMPL_API void ImGuiNewFrame();
        IMGUI_IMPL_API void ImGuiRenderDrawData(ImDrawData* drawData, VkCommandBuffer command_buffer, VkPipeline pipeline = VK_NULL_HANDLE);
        IMGUI_IMPL_API void ImGuiSetMinImageCount(uint32_t minImageCount); // To override MinImageCount after initialization (e.g. if swap chain is recreated)

        // (Advanced) Use e.g. if you need to recreate pipeline without reinitializing the backend (see #8110, #8111)
        // The main window pipeline will be created by ImGui_ImplVulkan_Init() if possible (== RenderPass xor (UseDynamicRendering && PipelineRenderingCreateInfo->sType == VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR))
        // Else, the pipeline can be created, or re-created, using ImGui_ImplVulkan_CreateMainPipeline() before rendering.
        IMGUI_IMPL_API void ImGuiCreateMainPipeline(VkPipelineRenderingCreateInfoKHR* pipelineRenderingCreateInfo);

        // (Advanced) Use e.g. if you need to precisely control the timing of texture updates (e.g. for staged rendering), by setting ImDrawData::Textures = nullptr to handle this manually.
        IMGUI_IMPL_API void ImGuiUpdateTexture(ImTextureData* tex);

        // Register a texture (VkDescriptorSet for a VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE == ImTextureID)
        IMGUI_IMPL_API VkDescriptorSet  ImGuiAddTexture(VkImageView imageView, VkImageLayout imageLayout);
        IMGUI_IMPL_API void             ImGuiRemoveTexture(VkDescriptorSet descriptorSet);
        IMGUI_IMPL_API bool             ImGuiCreateDeviceObjects();
        IMGUI_IMPL_API void             ImGuiDestroyDeviceObjects();
        IMGUI_IMPL_API void             ImGuiDestroyTexture(ImTextureData* tex);
        /**
         * @brief Start a new ImGui frame.
         */
        void NewFrame(uint32_t frameInFlight);

        /**
         * @brief Render the ImGui frame.
         * @param commandBuffer The command buffer to record rendering commands to.
         */
        void Render(uint32_t frameInFlight, const VkSemaphore& timelineSem, uint64_t signalValue, uint64_t waitValue);

        /**
         * @brief Update vertex and index buffers.
         */
        void UpdateBuffers(uint32_t frameInFlight);

        /**
         * @brief Add imgui widget draw calls just once at the start.
         */
        void AddPersistentDrawCalls(const std::function<void()>& func);

        /**
        * @brief Add imgui widget draw calls just once at the start called with frame index
        */
        void AddPersistentDrawCalls(const std::function<void(uint32_t frameIndex)>& func);

        // Input callbacks
        static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
        static void KeyCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods);
        static void WindowFocusCallback(GLFWwindow* window, int focused);
        static void CursorPosCallback(GLFWwindow* window, double x, double y);
        static void CursorEnterCallback(GLFWwindow* window, int entered);
        static void CharCallback(GLFWwindow* window, unsigned int c);
    };

}
#endif // !IMGUI_SYSTEM_H
