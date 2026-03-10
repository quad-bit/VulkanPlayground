#pragma once

#include <vulkan/vulkan.h>
#include <imgui.h>
#include <vector>
#include <glm/glm.hpp>
#include <functional>
#include <GLFW/glfw3.h>

// https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/GUI/02_imgui_setup.html#_creating_an_imgui_integration

namespace Common
{
    class ImguiUtil
    {
    private:

        ImGuiContext* m_imguiContext = nullptr;                    // ImGui context

        GLFWwindow* m_glfwWindow = nullptr;
        GLFWwindowfocusfun      m_userCallbackWindowFocus;
        GLFWcursorposfun        m_userCallbackCursorPos;
        GLFWcursorenterfun      m_userCallbackCursorEnter;
        GLFWmousebuttonfun      m_userCallbackMousebutton;
        GLFWscrollfun           m_userCallbackScroll;
        GLFWkeyfun              m_userCallbackKey;
        GLFWcharfun             m_userCallbackChar;

        static GLFWwindow* m_mouseWindow;
        static ImVec2                  m_lastValidMousePos;


        VkSampler m_sampler{ VK_NULL_HANDLE };                    // Texture sampling configuration for font rendering

        //VkBuffer m_combinedBuffer;                         // (index + vertex)*frameinflights
        //VkDeviceMemory m_combinedBufferMemory;
        std::vector<VkBuffer> m_vertexBuffers;                                    // Dynamic vertex buffer for UI geometry
        std::vector<VkBuffer> m_indexBuffers;                                     // Dynamic index buffer for UI triangle connectivity
        std::vector<VkDeviceMemory> m_vertexBufferMemories, m_indexBufferMemories;

        std::vector<uint32_t> m_vertexCounts;                         // Current vertex count for draw commands
        std::vector<uint32_t> m_indexCounts;                          // Current index count for draw commands
        VkImage m_fontImage;                                        // GPU texture containing ImGui font atlas
        VkImageView m_fontImageView;                              // Shader-accessible view of font texture
        VkDeviceMemory m_fontImageMemory;
        VkCommandPool m_commandPool;
        std::vector<VkCommandBuffer> m_commandBuffers;

        const std::vector<VkImageView>& m_colorAttachmentViews;
        std::vector<VkRenderingAttachmentInfo> m_colorList;
        std::vector<VkRenderingInfo> m_renderingInfoList;

        // Vulkan pipeline infrastructure for UI rendering
        // These objects define the complete GPU processing pipeline for ImGui elements
        VkPipelineCache m_pipelineCache{ VK_NULL_HANDLE };        // Pipeline compilation cache for faster startup
        VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };      // Resource binding layout (textures, uniforms)
        VkPipeline m_pipeline{ VK_NULL_HANDLE };                  // Complete graphics pipeline for UI rendering
        VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };      // Pool for allocating descriptor sets
        VkDescriptorSetLayout m_descriptorSetLayout{ VK_NULL_HANDLE }; // Layout defining shader resource bindings
        VkDescriptorSet m_descriptorSet{ VK_NULL_HANDLE };        // Actual resource bindings for font texture
        VkShaderModule m_shaderModuleVert = VK_NULL_HANDLE, m_shaderModuleFrag = VK_NULL_HANDLE;

        // Vulkan device context and system integration
        // These references connect our UI system to the broader Vulkan application context
        const VkDevice& cm_device = VK_NULL_HANDLE;                    // Primary Vulkan device for resource creation
        const VkPhysicalDevice& cm_physicalDevice = VK_NULL_HANDLE;    // GPU hardware info for capability queries
        const VkQueue& cm_graphicsQueue = VK_NULL_HANDLE;              // Command submission queue for UI rendering
        uint32_t m_graphicsQueueFamily = 0;                      // Queue family index for validation

        // UI state management and rendering configuration
        // These members control the visual appearance and dynamic behavior of the UI system
        ImGuiStyle m_vulkanStyle;                                 // Custom visual styling for Vulkan applications

        // Push constants for efficient per-frame parameter updates
        // This structure enables fast updates of transformation and styling data
        struct PushConstBlock
        {
            glm::vec2 scale;                                    // UI scaling factors for different screen sizes
            glm::vec2 translate;                                // Translation offset for UI positioning
        } m_pushConstBlock;

        // Dynamic state tracking for performance optimization
        bool m_needsUpdateBuffers = false;                        // Flag indicating buffer resize requirements

        // Modern Vulkan rendering configuration
        VkPipelineRenderingCreateInfo m_renderingInfo{};        // Dynamic rendering setup parameters
        VkFormat m_colorFormat = VkFormat::VK_FORMAT_B8G8R8A8_UNORM;   // Target framebuffer format
        uint32_t m_framebufferWidth = 100, m_framebufferHeight = 100;
        VkFormat m_depthFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
        bool m_initialized = false;
        bool m_frameAlreadyRendered = false;
        mutable std::vector<std::function<void()>> m_guiDrawPersistentList;

        // Resource creation
        void CreateFontTexture();
        void CreateDescriptorSetLayout();
        void CreateDescriptorPool();
        void CreateDescriptorSet();
        void CreatePiplelineLayout();
        void CreatePipeline();

    public:
        // Lifecycle management for proper resource initialization and cleanup
        ImguiUtil(GLFWwindow* window, const VkDevice& device, const VkPhysicalDevice& physicalDevice,
            const VkQueue& graphicsQueue, uint32_t graphicsQueueFamily, uint8_t frameInFlights,
            uint32_t frameBufferWidth, uint32_t framebufferHeight, VkFormat depthFormat, VkFormat colorFormat,
            const std::vector<VkImageView>& colorViews
        );
        ~ImguiUtil();

        // Input callbacks
        static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
        static void KeyCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods);
        static void WindowFocusCallback(GLFWwindow* window, int focused);
        static void CursorPosCallback(GLFWwindow* window, double x, double y);
        static void CursorEnterCallback(GLFWwindow* window, int entered);
        static void CharCallback(GLFWwindow* window, unsigned int c);

        // Core functionality methods for ImGui integration
        void Init();                                             // Initialize ImGui context and configure display
        void InitResources();                                    // Create all Vulkan resources for rendering
        void SetStyle(uint32_t index);                          // Apply visual styling themes
        void Cleanup();

        /**
         * @brief Add imgui widget draw calls just once at the start.
         */
        void AddPersistentDrawCalls(const std::function<void()>& func) const;

        /**
         * @brief Start a new ImGui frame.
         */
        void NewFrame();

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
         * @brief Handle mouse input.
         * @param x The x-coordinate of the mouse.
         * @param y The y-coordinate of the mouse.
         * @param buttons The state of the mouse buttons.
         */
        void HandleMouse(float x, float y, uint32_t buttons);

        /**
         * @brief Handle keyboard input.
         * @param key The key code.
         * @param pressed Whether the key was pressed or released.
         */
        void HandleKeyboard(uint32_t key, bool pressed);

        /**
         * @brief Handle character input.
         * @param c The character.
         */
        void HandleChar(uint32_t c);

        /**
         * @brief Handle window resize.
         * @param width The new width of the window.
         * @param height The new height of the window.
         */
        void HandleResize(uint32_t width, uint32_t height);

        /**
         * @brief Check if ImGui wants to capture keyboard input.
         * @return True if ImGui wants to capture keyboard input, false otherwise.
         */
        bool WantCaptureKeyboard() const;

        /**
         * @brief Check if ImGui wants to capture mouse input.
         * @return True if ImGui wants to capture mouse input, false otherwise.
         */
        bool WantCaptureMouse() const;

        /**
         * @brief Check if ImGui has already been rendered for the current frame.
         * @return True if Render() was already called in NewFrame(), false otherwise.
         */
        bool IsFrameRendered() const
        {
            return m_frameAlreadyRendered;
        }
    };
}
