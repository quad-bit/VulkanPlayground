#include "ImguiUtil.h"
#include "Utils.h"
#include "plog/Log.h"
#include <array>
#include <ImguiGlfwHelper.h>


GLFWwindow*  Common::ImguiUtil::m_mouseWindow = nullptr;
ImVec2       Common::ImguiUtil::m_lastValidMousePos = ImVec2(0,0);

namespace 
{
    // imgui_impl_vulkan.cpp from imgui samples
    /*
    #version 450 core
    layout(location = 0) in vec2 aPos;
    layout(location = 1) in vec2 aUV;
    layout(location = 2) in vec4 aColor;
    layout(push_constant) uniform uPushConstant { vec2 uScale; vec2 uTranslate; } pc;

    out gl_PerVertex { vec4 gl_Position; };
    layout(location = 0) out struct { vec4 Color; vec2 UV; } Out;

    void main()
    {
        Out.Color = aColor;
        Out.UV = aUV;
        gl_Position = vec4(aPos * pc.uScale + pc.uTranslate, 0, 1);
    }
    */
    static uint32_t __glsl_shader_vert_spv[] =
    {
        0x07230203,0x00010000,0x00080001,0x0000002e,0x00000000,0x00020011,0x00000001,0x0006000b,
        0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
        0x000a000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x0000000b,0x0000000f,0x00000015,
        0x0000001b,0x0000001c,0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,0x6e69616d,
        0x00000000,0x00030005,0x00000009,0x00000000,0x00050006,0x00000009,0x00000000,0x6f6c6f43,
        0x00000072,0x00040006,0x00000009,0x00000001,0x00005655,0x00030005,0x0000000b,0x0074754f,
        0x00040005,0x0000000f,0x6c6f4361,0x0000726f,0x00030005,0x00000015,0x00565561,0x00060005,
        0x00000019,0x505f6c67,0x65567265,0x78657472,0x00000000,0x00060006,0x00000019,0x00000000,
        0x505f6c67,0x7469736f,0x006e6f69,0x00030005,0x0000001b,0x00000000,0x00040005,0x0000001c,
        0x736f5061,0x00000000,0x00060005,0x0000001e,0x73755075,0x6e6f4368,0x6e617473,0x00000074,
        0x00050006,0x0000001e,0x00000000,0x61635375,0x0000656c,0x00060006,0x0000001e,0x00000001,
        0x61725475,0x616c736e,0x00006574,0x00030005,0x00000020,0x00006370,0x00040047,0x0000000b,
        0x0000001e,0x00000000,0x00040047,0x0000000f,0x0000001e,0x00000002,0x00040047,0x00000015,
        0x0000001e,0x00000001,0x00050048,0x00000019,0x00000000,0x0000000b,0x00000000,0x00030047,
        0x00000019,0x00000002,0x00040047,0x0000001c,0x0000001e,0x00000000,0x00050048,0x0000001e,
        0x00000000,0x00000023,0x00000000,0x00050048,0x0000001e,0x00000001,0x00000023,0x00000008,
        0x00030047,0x0000001e,0x00000002,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,
        0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,0x00000004,0x00040017,
        0x00000008,0x00000006,0x00000002,0x0004001e,0x00000009,0x00000007,0x00000008,0x00040020,
        0x0000000a,0x00000003,0x00000009,0x0004003b,0x0000000a,0x0000000b,0x00000003,0x00040015,
        0x0000000c,0x00000020,0x00000001,0x0004002b,0x0000000c,0x0000000d,0x00000000,0x00040020,
        0x0000000e,0x00000001,0x00000007,0x0004003b,0x0000000e,0x0000000f,0x00000001,0x00040020,
        0x00000011,0x00000003,0x00000007,0x0004002b,0x0000000c,0x00000013,0x00000001,0x00040020,
        0x00000014,0x00000001,0x00000008,0x0004003b,0x00000014,0x00000015,0x00000001,0x00040020,
        0x00000017,0x00000003,0x00000008,0x0003001e,0x00000019,0x00000007,0x00040020,0x0000001a,
        0x00000003,0x00000019,0x0004003b,0x0000001a,0x0000001b,0x00000003,0x0004003b,0x00000014,
        0x0000001c,0x00000001,0x0004001e,0x0000001e,0x00000008,0x00000008,0x00040020,0x0000001f,
        0x00000009,0x0000001e,0x0004003b,0x0000001f,0x00000020,0x00000009,0x00040020,0x00000021,
        0x00000009,0x00000008,0x0004002b,0x00000006,0x00000028,0x00000000,0x0004002b,0x00000006,
        0x00000029,0x3f800000,0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,
        0x00000005,0x0004003d,0x00000007,0x00000010,0x0000000f,0x00050041,0x00000011,0x00000012,
        0x0000000b,0x0000000d,0x0003003e,0x00000012,0x00000010,0x0004003d,0x00000008,0x00000016,
        0x00000015,0x00050041,0x00000017,0x00000018,0x0000000b,0x00000013,0x0003003e,0x00000018,
        0x00000016,0x0004003d,0x00000008,0x0000001d,0x0000001c,0x00050041,0x00000021,0x00000022,
        0x00000020,0x0000000d,0x0004003d,0x00000008,0x00000023,0x00000022,0x00050085,0x00000008,
        0x00000024,0x0000001d,0x00000023,0x00050041,0x00000021,0x00000025,0x00000020,0x00000013,
        0x0004003d,0x00000008,0x00000026,0x00000025,0x00050081,0x00000008,0x00000027,0x00000024,
        0x00000026,0x00050051,0x00000006,0x0000002a,0x00000027,0x00000000,0x00050051,0x00000006,
        0x0000002b,0x00000027,0x00000001,0x00070050,0x00000007,0x0000002c,0x0000002a,0x0000002b,
        0x00000028,0x00000029,0x00050041,0x00000011,0x0000002d,0x0000001b,0x0000000d,0x0003003e,
        0x0000002d,0x0000002c,0x000100fd,0x00010038
    };

    // backends/vulkan/glsl_shader.frag, compiled with:
    // # glslangValidator -V -x -o glsl_shader.frag.u32 glsl_shader.frag
    /*
    #version 450 core
    layout(location = 0) out vec4 fColor;
    layout(set=0, binding=0) uniform sampler2D sTexture;
    layout(location = 0) in struct { vec4 Color; vec2 UV; } In;
    void main()
    {
        fColor = In.Color * texture(sTexture, In.UV.st);
    }
    */
    static uint32_t __glsl_shader_frag_spv[] =
    {
        0x07230203,0x00010000,0x00080001,0x0000001e,0x00000000,0x00020011,0x00000001,0x0006000b,
        0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
        0x0007000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000d,0x00030010,
        0x00000004,0x00000007,0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,0x6e69616d,
        0x00000000,0x00040005,0x00000009,0x6c6f4366,0x0000726f,0x00030005,0x0000000b,0x00000000,
        0x00050006,0x0000000b,0x00000000,0x6f6c6f43,0x00000072,0x00040006,0x0000000b,0x00000001,
        0x00005655,0x00030005,0x0000000d,0x00006e49,0x00050005,0x00000016,0x78655473,0x65727574,
        0x00000000,0x00040047,0x00000009,0x0000001e,0x00000000,0x00040047,0x0000000d,0x0000001e,
        0x00000000,0x00040047,0x00000016,0x00000022,0x00000000,0x00040047,0x00000016,0x00000021,
        0x00000000,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,
        0x00000020,0x00040017,0x00000007,0x00000006,0x00000004,0x00040020,0x00000008,0x00000003,
        0x00000007,0x0004003b,0x00000008,0x00000009,0x00000003,0x00040017,0x0000000a,0x00000006,
        0x00000002,0x0004001e,0x0000000b,0x00000007,0x0000000a,0x00040020,0x0000000c,0x00000001,
        0x0000000b,0x0004003b,0x0000000c,0x0000000d,0x00000001,0x00040015,0x0000000e,0x00000020,
        0x00000001,0x0004002b,0x0000000e,0x0000000f,0x00000000,0x00040020,0x00000010,0x00000001,
        0x00000007,0x00090019,0x00000013,0x00000006,0x00000001,0x00000000,0x00000000,0x00000000,
        0x00000001,0x00000000,0x0003001b,0x00000014,0x00000013,0x00040020,0x00000015,0x00000000,
        0x00000014,0x0004003b,0x00000015,0x00000016,0x00000000,0x0004002b,0x0000000e,0x00000018,
        0x00000001,0x00040020,0x00000019,0x00000001,0x0000000a,0x00050036,0x00000002,0x00000004,
        0x00000000,0x00000003,0x000200f8,0x00000005,0x00050041,0x00000010,0x00000011,0x0000000d,
        0x0000000f,0x0004003d,0x00000007,0x00000012,0x00000011,0x0004003d,0x00000014,0x00000017,
        0x00000016,0x00050041,0x00000019,0x0000001a,0x0000000d,0x00000018,0x0004003d,0x0000000a,
        0x0000001b,0x0000001a,0x00050057,0x00000007,0x0000001c,0x00000017,0x0000001b,0x00050085,
        0x00000007,0x0000001d,0x00000012,0x0000001c,0x0003003e,0x00000009,0x0000001d,0x000100fd,
        0x00010038
    };


};

Common::ImguiUtil::ImguiUtil(GLFWwindow* glfwWindow, const VkDevice& device, const VkPhysicalDevice& physicalDevice, const VkQueue& graphicsQueue,
    uint32_t graphicsQueueFamily, uint8_t frameInFlights, uint32_t frameBufferWidth, uint32_t framebufferHeight, VkFormat depthFormat, 
    VkFormat colorFormat, const std::vector<VkImageView>& colorViews):
    m_glfwWindow(glfwWindow), cm_device(device), cm_physicalDevice(physicalDevice), cm_graphicsQueue(graphicsQueue), m_graphicsQueueFamily(graphicsQueueFamily),
    m_framebufferWidth(frameBufferWidth), m_framebufferHeight(framebufferHeight), m_colorFormat(colorFormat), m_depthFormat(depthFormat), m_colorAttachmentViews(colorViews)
{
    m_userCallbackWindowFocus = glfwSetWindowFocusCallback(m_glfwWindow, WindowFocusCallback);
    m_userCallbackCursorEnter = glfwSetCursorEnterCallback(m_glfwWindow, CursorEnterCallback);
    m_userCallbackCursorPos = glfwSetCursorPosCallback(m_glfwWindow, CursorPosCallback);
    m_userCallbackMousebutton = glfwSetMouseButtonCallback(m_glfwWindow, MouseButtonCallback);
    m_userCallbackScroll = glfwSetScrollCallback(m_glfwWindow, ScrollCallback);
    m_userCallbackKey = glfwSetKeyCallback(m_glfwWindow, KeyCallback);
    m_userCallbackChar = glfwSetCharCallback(m_glfwWindow, CharCallback);

    m_vertexBuffers.resize(frameInFlights);
    m_vertexBufferMemories.resize(frameInFlights);

    m_indexBuffers.resize(frameInFlights);
    m_indexBufferMemories.resize(frameInFlights);

    size_t initialSize = 1024;

    for (uint8_t i = 0; i < frameInFlights; i++)
    {
        /*CreateBufferAndMemory(physicalDevice, device, m_combinedBuffer, m_combinedBufferMemory, initialSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);*/
        CreateBufferAndMemory(physicalDevice, device, m_vertexBuffers[i], m_vertexBufferMemories[i], initialSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        CreateBufferAndMemory(physicalDevice, device, m_indexBuffers[i], m_indexBufferMemories[i], initialSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT| VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
    m_vertexCounts.assign(frameInFlights, 0);
    m_indexCounts.assign(frameInFlights, 0);

    VkCommandPoolCreateInfo createInfo{};
    createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = m_graphicsQueueFamily;
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    ErrorCheck(vkCreateCommandPool(device, &createInfo, nullptr, &m_commandPool));

    m_commandBuffers.resize(frameInFlights);
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.commandBufferCount = frameInFlights;
    alloc_info.commandPool = m_commandPool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

    ErrorCheck(vkAllocateCommandBuffers(device, &alloc_info, &m_commandBuffers[0]));

    m_colorList.resize(frameInFlights);
    for (uint32_t i = 0; i < frameInFlights; i++)
    {
        m_colorList[i].imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        m_colorList[i].imageView = m_colorAttachmentViews[i];
        m_colorList[i].loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_LOAD;
        m_colorList[i].storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
        m_colorList[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

        VkRenderingInfo info{};
        info.colorAttachmentCount = (1);
        info.layerCount = (1);
        info.pColorAttachments = &m_colorList[i];
        info.renderArea = VkRect2D{ {0, 0}, {(uint32_t)frameBufferWidth, framebufferHeight} };
        info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        m_renderingInfoList.push_back(std::move(info));
    }
}

Common::ImguiUtil::~ImguiUtil()
{
}

void Common::ImguiUtil::Init()
{
    if (m_initialized)
    {
        PLOGD<<"Already initialised" ;
        return ;
    }

    // Create ImGui context
    m_imguiContext = ImGui::CreateContext();
    if (!m_imguiContext)
    {
        PLOG_ERROR << "Failed to create ImGui context" << std::endl;
        return ;
    }

    // Configure ImGui
    ImGuiIO& io = ImGui::GetIO();
    // Set display size
    io.DisplaySize = ImVec2(static_cast<float>(m_framebufferWidth), static_cast<float>(m_framebufferHeight));
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

    InitResources();

    m_initialized = true;
}

void Common::ImguiUtil::InitResources()
{
    CreateFontTexture();
    CreateDescriptorSetLayout();
    CreateDescriptorPool();
    CreateDescriptorSet();
    CreatePiplelineLayout();
    CreatePipeline();
}

void Common::ImguiUtil::SetStyle(uint32_t index)
{
}

void Common::ImguiUtil::Cleanup()
{
    if (!m_initialized)
    {
        return;
    }

    // Wait for the device to be idle before cleaning up
    vkDeviceWaitIdle(cm_device);

    {
        for (uint32_t i = 0; i < m_vertexBufferMemories.size(); i++)
        {
            vkFreeMemory(cm_device, m_vertexBufferMemories[i], nullptr);
            vkFreeMemory(cm_device, m_indexBufferMemories[i], nullptr);
            vkDestroyBuffer(cm_device, m_vertexBuffers[i], nullptr);
            vkDestroyBuffer(cm_device, m_indexBuffers[i], nullptr);
        }

        vkDestroyImage(cm_device, m_fontImage, nullptr);
        vkFreeMemory(cm_device, m_fontImageMemory, nullptr);
        vkDestroyImageView(cm_device, m_fontImageView, nullptr);
        vkDestroySampler(cm_device, m_sampler, nullptr);

        //vkFreeMemory(cm_device, m_combinedBufferMemory, nullptr);
        //vkDestroyBuffer(cm_device, m_combinedBuffer, nullptr);

        vkDestroyDescriptorSetLayout(cm_device, m_descriptorSetLayout, nullptr);
        vkDestroyDescriptorPool(cm_device, m_descriptorPool, nullptr);
        vkDestroyPipelineLayout(cm_device, m_pipelineLayout, nullptr);
        vkDestroyShaderModule(cm_device, m_shaderModuleVert, nullptr);
        vkDestroyShaderModule(cm_device, m_shaderModuleFrag, nullptr);
        vkDestroyPipeline(cm_device, m_pipeline, nullptr);

        vkDestroyCommandPool(cm_device, m_commandPool, nullptr);
    }

    // Destroy ImGui context
    if (m_imguiContext)
    {
        ImGui::DestroyContext(m_imguiContext);
        m_imguiContext = nullptr;
    }

    m_initialized = false;
}

void Common::ImguiUtil::AddPersistentDrawCalls(const std::function<void()>& func) const
{
    m_guiDrawPersistentList.push_back(func);
}

void Common::ImguiUtil::NewFrame()
{
    if (!m_initialized)
    {
        return;
    }

    // Reset the flag at the start of each frame
    m_frameAlreadyRendered = false;

    ImGui::NewFrame();

    for (auto& func : m_guiDrawPersistentList)
    {
        func();
    }
}


void Common::ImguiUtil::Render(uint32_t frameInFlight, const VkSemaphore& timelineSem, uint64_t signalValue, uint64_t waitValue)
{
    if (!m_initialized)
    {
        return;
    }

    // End the frame and prepare for rendering
    ImGui::Render();

    // Update vertex and index buffers for this frame
    UpdateBuffers(frameInFlight);

    // Record rendering commands
    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData || drawData->CmdListsCount == 0)
    {
        {
            VkSemaphoreSubmitInfo signalInfo
            { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, nullptr, timelineSem, signalValue, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0 };

            VkCommandBufferSubmitInfo bufInfo{};
            bufInfo.commandBuffer = m_commandBuffers[frameInFlight];
            bufInfo.deviceMask = 0;
            bufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;

            VkSubmitInfo2 submitInfo{};
            submitInfo.commandBufferInfoCount = 0;
            submitInfo.pSignalSemaphoreInfos = &signalInfo;
            submitInfo.signalSemaphoreInfoCount = 1;
            submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;

            // If the threads are being killed, we need to skip the queue submission to allow the program to exit gracefully
            //if (m_alive)
            {
                ErrorCheck(vkQueueSubmit2(cm_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
            }
        }
        return;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    ErrorCheck(vkBeginCommandBuffer(m_commandBuffers[frameInFlight], &beginInfo));

    vkCmdBeginRendering(m_commandBuffers[frameInFlight], &m_renderingInfoList[frameInFlight]);
    {
        // Bind the pipeline
        vkCmdBindPipeline(m_commandBuffers[frameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

        // Set viewport
        VkViewport viewport;
        viewport.width = ImGui::GetIO().DisplaySize.x;
        viewport.height = ImGui::GetIO().DisplaySize.y;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        viewport.x = 0.0f;
        viewport.y = 0.0f;

        vkCmdSetViewport(m_commandBuffers[frameInFlight], 0, 1, &viewport);

        m_pushConstBlock.scale[0] = 2.0f / ImGui::GetIO().DisplaySize.x;
        m_pushConstBlock.scale[1] = 2.0f / ImGui::GetIO().DisplaySize.y;
        m_pushConstBlock.translate[0] = -1.0f;
        m_pushConstBlock.translate[1] = -1.0f;

        vkCmdPushConstants(m_commandBuffers[frameInFlight], m_pipelineLayout, VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstBlock), &m_pushConstBlock);
        //commandBuffer.pushConstants<PushConstBlock>(*pipelineLayout, vk::ShaderStageFlagBits::eVertex, 0, pushConstBlock);

        // Bind vertex and index buffers for this frame
        VkDeviceSize offsets[]{ 0 };
        vkCmdBindVertexBuffers(m_commandBuffers[frameInFlight], 0, 1, &m_vertexBuffers[frameInFlight], offsets);
        vkCmdBindIndexBuffer(m_commandBuffers[frameInFlight], m_indexBuffers[frameInFlight], 0, VK_INDEX_TYPE_UINT16);
        //commandBuffer.bindVertexBuffers(0, *vertexBuffers[frameInFlight], vk::DeviceSize{ 0 });
        //commandBuffer.bindIndexBuffer(*indexBuffers[frameInFlight], 0, vk::IndexType::eUint16);

        // Render command lists
        int vertexOffset = 0;
        int indexOffset = 0;

        for (int i = 0; i < drawData->CmdListsCount; i++)
        {
            const ImDrawList* cmdList = drawData->CmdLists[i];

            for (int j = 0; j < cmdList->CmdBuffer.Size; j++)
            {
                const ImDrawCmd* pcmd = &cmdList->CmdBuffer[j];

                // Set scissor rectangle
                VkRect2D scissor;
                scissor.offset.x = std::max(static_cast<int32_t>(pcmd->ClipRect.x), 0);
                scissor.offset.y = std::max(static_cast<int32_t>(pcmd->ClipRect.y), 0);
                scissor.extent.width = static_cast<uint32_t>(pcmd->ClipRect.z - pcmd->ClipRect.x);
                scissor.extent.height = static_cast<uint32_t>(pcmd->ClipRect.w - pcmd->ClipRect.y);
                vkCmdSetScissor(m_commandBuffers[frameInFlight], 0, 1, &scissor);
                //commandBuffer.setScissor(0, { scissor });

                // Bind descriptor set (font texture)
                vkCmdBindDescriptorSets(m_commandBuffers[frameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
                //commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, { *descriptorSet }, {});

                // Draw
                vkCmdDrawIndexed(m_commandBuffers[frameInFlight], pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
                //commandBuffer.drawIndexed(pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
                indexOffset += pcmd->ElemCount;
            }
            vertexOffset += cmdList->VtxBuffer.Size;
        }
    }
    vkCmdEndRendering(m_commandBuffers[frameInFlight]);

    ErrorCheck(vkEndCommandBuffer(m_commandBuffers[frameInFlight]));

    {
        VkSemaphoreSubmitInfo waitInfo
        { VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, nullptr, timelineSem, waitValue, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0 };

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
        submitInfo.pWaitSemaphoreInfos = &waitInfo;
        submitInfo.signalSemaphoreInfoCount = 1;
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submitInfo.waitSemaphoreInfoCount = 1;

        // If the threads are being killed, we need to skip the queue submission to allow the program to exit gracefully
        //if (m_alive)
        {
            ErrorCheck(vkQueueSubmit2(cm_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE));
        }
    }

}

void Common::ImguiUtil::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    //ImGuiIO& io = ImGui::GetIO(bd->Context);
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseWheelEvent((float)xoffset, (float)yoffset);
}

void Common::ImguiUtil::KeyCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods)
{
    if (action != GLFW_PRESS && action != GLFW_RELEASE)
        return;

    //ImGuiIO& io = ImGui::GetIO(bd->Context);
    ImGuiIO& io = ImGui::GetIO();
    UpdateKeyModifiers(io, window);

    keycode = TranslateUntranslatedKey(keycode, scancode);

    ImGuiKey imgui_key = KeyToImGuiKey(keycode, scancode);
    io.AddKeyEvent(imgui_key, (action == GLFW_PRESS));
    io.SetKeyEventNativeData(imgui_key, keycode, scancode); // To support legacy indexing (<1.87 user code)
}

void Common::ImguiUtil::WindowFocusCallback(GLFWwindow* window, int focused)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddFocusEvent(focused != 0);
}

void Common::ImguiUtil::CursorPosCallback(GLFWwindow* window, double x, double y)
{
    //ImGuiIO& io = ImGui::GetIO(bd->Context);
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent((float)x, (float)y);
    m_lastValidMousePos = ImVec2((float)x, (float)y);
}

void Common::ImguiUtil::CursorEnterCallback(GLFWwindow* window, int entered)
{
    ImGuiIO& io = ImGui::GetIO();
    if (entered)
    {
        m_mouseWindow = window;
        io.AddMousePosEvent(m_lastValidMousePos.x, m_lastValidMousePos.y);
    }
    else if (!entered && m_mouseWindow == window)
    {
        m_lastValidMousePos = io.MousePos;
        m_mouseWindow = nullptr;
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    }
}

void Common::ImguiUtil::CharCallback(GLFWwindow* window, unsigned int c)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddInputCharacter(c);
}

void Common::ImguiUtil::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    ImGuiIO& io = ImGui::GetIO();
    UpdateKeyModifiers(io, window);
    if (button >= 0 && button < ImGuiMouseButton_COUNT)
        io.AddMouseButtonEvent(button, action == GLFW_PRESS);
}

void Common::ImguiUtil::UpdateBuffers(uint32_t frameInFlight)
{
    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData || drawData->CmdListsCount == 0)
    {
        return;
    }

    // Calculate required buffer sizes
    VkDeviceSize vertexBufferSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
    VkDeviceSize indexBufferSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);

    // Resize buffers if needed for this frame
    if (frameInFlight >= m_vertexCounts.size())
        return; // Safety

    if (static_cast<uint32_t>(drawData->TotalVtxCount) > m_vertexCounts[frameInFlight]) 
    {
        // Clean up old buffer
        DestroyBuffer(cm_device, m_vertexBuffers[frameInFlight]);
        FreeMemory(cm_device, m_vertexBufferMemories[frameInFlight]);

        CreateBufferAndMemory(cm_physicalDevice, cm_device, m_vertexBuffers[frameInFlight], m_vertexBufferMemories[frameInFlight], vertexBufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        m_vertexCounts[frameInFlight] = drawData->TotalVtxCount;
    }

    if (static_cast<uint32_t>(drawData->TotalIdxCount) > m_indexCounts[frameInFlight]) 
    {
        // Clean up old buffer
        DestroyBuffer(cm_device, m_indexBuffers[frameInFlight]);
        FreeMemory(cm_device, m_indexBufferMemories[frameInFlight]);

        CreateBufferAndMemory(cm_physicalDevice, cm_device, m_indexBuffers[frameInFlight], m_indexBufferMemories[frameInFlight], indexBufferSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        m_indexCounts[frameInFlight] = drawData->TotalIdxCount;
    }

    // Upload data to buffers for this frame (only if we have data to upload)
    if (drawData->TotalVtxCount > 0 && drawData->TotalIdxCount > 0) 
    {
        void *vtxMappedMemory, *idxMappedMemory;
        ErrorCheck(vkMapMemory(cm_device, m_vertexBufferMemories[frameInFlight], 0, vertexBufferSize, 0, &vtxMappedMemory));
        ErrorCheck(vkMapMemory(cm_device, m_indexBufferMemories[frameInFlight], 0, indexBufferSize, 0, &idxMappedMemory));
        //void* vtxMappedMemory = vertexBufferMemories[frameInFlight].mapMemory(0, vertexBufferSize);
        //void* idxMappedMemory = indexBufferMemories[frameInFlight].mapMemory(0, indexBufferSize);

        ImDrawVert* vtxDst = static_cast<ImDrawVert*>(vtxMappedMemory);
        ImDrawIdx* idxDst = static_cast<ImDrawIdx*>(idxMappedMemory);

        for (int n = 0; n < drawData->CmdListsCount; n++) 
        {
            const ImDrawList* cmdList = drawData->CmdLists[n];
            memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtxDst += cmdList->VtxBuffer.Size;
            idxDst += cmdList->IdxBuffer.Size;
        }

        vkUnmapMemory(cm_device, m_vertexBufferMemories[frameInFlight]);
        vkUnmapMemory(cm_device, m_indexBufferMemories[frameInFlight]);
        //vertexBufferMemories[frameInFlight].unmapMemory();
        //indexBufferMemories[frameInFlight].unmapMemory();
    }
}

void Common::ImguiUtil::HandleMouse(float x, float y, uint32_t buttons)
{
    if (!m_initialized) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    // Update mouse position
    io.MousePos = ImVec2(x, y);

    // Update mouse buttons
    io.MouseDown[0] = (buttons & 0x01) != 0; // Left button
    io.MouseDown[1] = (buttons & 0x02) != 0; // Right button
    io.MouseDown[2] = (buttons & 0x04) != 0; // Middle button
}

void Common::ImguiUtil::HandleKeyboard(uint32_t key, bool pressed)
{
    if (!m_initialized) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    //// Update key state
    //if (key < 512) {
    //    io.KeysDown[key] = pressed;
    //}

    //// Update modifier keys
    //// Using GLFW key codes instead of Windows-specific VK_* constants
    //io.KeyCtrl = io.KeysDown[341] || io.KeysDown[345]; // Left/Right Control
    //io.KeyShift = io.KeysDown[340] || io.KeysDown[344]; // Left/Right Shift
    //io.KeyAlt = io.KeysDown[342] || io.KeysDown[346]; // Left/Right Alt
    //io.KeySuper = io.KeysDown[343] || io.KeysDown[347]; // Left/Right Super
}

void Common::ImguiUtil::HandleChar(uint32_t c)
{
    if (!m_initialized) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.AddInputCharacter(c);
}

void Common::ImguiUtil::HandleResize(uint32_t width, uint32_t height)
{
    if (!m_initialized) {
        return;
    }

    this->m_framebufferWidth = width;
    this->m_framebufferHeight = height;

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(width), static_cast<float>(height));
}

bool Common::ImguiUtil::WantCaptureKeyboard() const
{
    if (!m_initialized) {
        return false;
    }

    return ImGui::GetIO().WantCaptureKeyboard;
}

bool Common::ImguiUtil::WantCaptureMouse() const
{
    if (!m_initialized) {
        return false;
    }

    return ImGui::GetIO().WantCaptureMouse;
}

void Common::ImguiUtil::CreateFontTexture()
{
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* fontData;
    int texWidth, texHeight;
    io.Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);
    VkDeviceSize uploadSize = texWidth * texHeight * 4 * sizeof(char);

    auto result = CreateImage(cm_device, cm_physicalDevice, texWidth, texHeight, m_colorFormat,
        VkImageUsageFlagBits::VK_IMAGE_USAGE_SAMPLED_BIT | VkImageUsageFlagBits::VK_IMAGE_USAGE_TRANSFER_DST_BIT);

    m_fontImage = std::get<0>(result);
    m_fontImageMemory = std::get<1>(result);

    auto [stagingBuffer, stagingBufferMemory] = LoadImageDataIntoStagingBuffer(cm_physicalDevice, cm_device, fontData, uploadSize);

    std::vector<VkImage> imageList{ m_fontImage };
    ChangeImageLayout(cm_device, imageList, cm_graphicsQueue, m_graphicsQueueFamily, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandPoolCreateInfo info{};
        info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        info.pNext = nullptr;
        info.queueFamilyIndex = m_graphicsQueueFamily;
        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

        ErrorCheck(vkCreateCommandPool(cm_device, &info, nullptr, &pool));

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.commandBufferCount = 1;
        allocInfo.commandPool = pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.pNext = nullptr;
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

        VkCommandBuffer cmdBuffer = VK_NULL_HANDLE;
        ErrorCheck(vkAllocateCommandBuffers(cm_device, &allocInfo, &cmdBuffer));

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = nullptr;
        beginInfo.pNext = nullptr;
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        vkBeginCommandBuffer(cmdBuffer, &beginInfo);
        CopyBufferToImage(cmdBuffer, stagingBuffer, m_fontImage, texWidth, texHeight);
        vkEndCommandBuffer(cmdBuffer);

        VkFence fence = VK_NULL_HANDLE;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.pNext = nullptr;
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        ErrorCheck(vkCreateFence(cm_device, &fenceInfo, nullptr, &fence));

        VkSubmitInfo submitInfo{};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;
        submitInfo.pNext = nullptr;
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        ErrorCheck(vkQueueSubmit(cm_graphicsQueue, 1, &submitInfo, fence));

        ErrorCheck(vkWaitForFences(cm_device, 1, &fence, VK_TRUE, UINT64_MAX));

        vkDestroyFence(cm_device, fence, nullptr);
        vkDestroyCommandPool(cm_device, pool, nullptr);
    }
    ChangeImageLayout(cm_device, imageList, cm_graphicsQueue, m_graphicsQueueFamily, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Staging buffer and memory clean up
    DestroyBuffer(cm_device, stagingBuffer);
    FreeMemory(cm_device, stagingBufferMemory);

    m_fontImageView = CreateImageView(cm_device, cm_physicalDevice, m_fontImage, m_colorFormat, VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT);

    // Create sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    ErrorCheck(vkCreateSampler(cm_device, &samplerInfo, nullptr, &m_sampler));
}

void Common::ImguiUtil::CreateDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding binding{};
    binding.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;
    binding.binding = 0;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    ErrorCheck(vkCreateDescriptorSetLayout(cm_device, &layoutInfo, nullptr, &m_descriptorSetLayout));
}

void Common::ImguiUtil::CreateDescriptorPool()
{
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.flags = VkDescriptorPoolCreateFlagBits::VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;

    ErrorCheck(vkCreateDescriptorPool(cm_device, &poolInfo, nullptr, &m_descriptorPool));
}

void Common::ImguiUtil::CreateDescriptorSet()
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ErrorCheck(vkAllocateDescriptorSets(cm_device, &allocInfo, &m_descriptorSet));

    PLOGD << "ImGui created descriptor set with handle: " << m_descriptorSet << std::endl;

    // Update descriptor set
    VkDescriptorImageInfo imageInfo;
    imageInfo.imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = m_fontImageView;
    imageInfo.sampler = m_sampler;

    VkWriteDescriptorSet writeSet{};
    writeSet.dstSet = m_descriptorSet;
    writeSet.descriptorCount = 1;
    writeSet.descriptorType = VkDescriptorType::VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeSet.pImageInfo = &imageInfo;
    writeSet.dstBinding = 0;
    writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    vkUpdateDescriptorSets(cm_device, 1, &writeSet, 0, nullptr);
}

void Common::ImguiUtil::CreatePiplelineLayout()
{
    VkPushConstantRange pushConstantRange;
    pushConstantRange.stageFlags = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(float) * 4; // 2 floats for scale, 2 floats for translate

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &m_descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    ErrorCheck(vkCreatePipelineLayout(cm_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout));
}

void Common::ImguiUtil::CreatePipeline()
{
    VkShaderModuleCreateInfo vertexShaderModuleInfo = {};
    vertexShaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vertexShaderModuleInfo.codeSize = sizeof(__glsl_shader_vert_spv);
    vertexShaderModuleInfo.pCode = (uint32_t*)__glsl_shader_vert_spv;
    ErrorCheck(vkCreateShaderModule(cm_device, &vertexShaderModuleInfo, nullptr, &m_shaderModuleVert));

    VkShaderModuleCreateInfo fragmentShaderModuleInfo = {};
    fragmentShaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    fragmentShaderModuleInfo.codeSize = sizeof(__glsl_shader_frag_spv);
    fragmentShaderModuleInfo.pCode = (uint32_t*)__glsl_shader_frag_spv;
    ErrorCheck(vkCreateShaderModule(cm_device, &fragmentShaderModuleInfo, nullptr, &m_shaderModuleFrag));

    // Shader stage creation
    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = m_shaderModuleVert;
    vertShaderStageInfo.pName = "main";
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.stage = VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = m_shaderModuleFrag;
    fragShaderStageInfo.pName = "main";
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

    std::array shaderStages = { vertShaderStageInfo, fragShaderStageInfo };

    VkVertexInputBindingDescription binding_desc[1] = {};
    binding_desc[0].stride = sizeof(ImDrawVert);
    binding_desc[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attribute_desc[3] = {};
    attribute_desc[0].location = 0;
    attribute_desc[0].binding = binding_desc[0].binding;
    attribute_desc[0].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_desc[0].offset = offsetof(ImDrawVert, pos);
    attribute_desc[1].location = 1;
    attribute_desc[1].binding = binding_desc[0].binding;
    attribute_desc[1].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_desc[1].offset = offsetof(ImDrawVert, uv);
    attribute_desc[2].location = 2;
    attribute_desc[2].binding = binding_desc[0].binding;
    attribute_desc[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    attribute_desc[2].offset = offsetof(ImDrawVert, col);

    VkPipelineVertexInputStateCreateInfo vertex_info = {};
    vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_info.vertexBindingDescriptionCount = 1;
    vertex_info.pVertexBindingDescriptions = binding_desc;
    vertex_info.vertexAttributeDescriptionCount = 3;
    vertex_info.pVertexAttributeDescriptions = attribute_desc;

    VkPipelineInputAssemblyStateCreateInfo ia_info = {};
    ia_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport_info = {};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.scissorCount = 1;


    VkPipelineRasterizationStateCreateInfo raster_info = {};
    raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster_info.polygonMode = VK_POLYGON_MODE_FILL;
    raster_info.cullMode = VK_CULL_MODE_NONE;
    raster_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster_info.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms_info = {};
    ms_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_attachment[1] = {};
    color_attachment[0].blendEnable = VK_TRUE;
    color_attachment[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_attachment[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment[0].colorBlendOp = VK_BLEND_OP_ADD;
    color_attachment[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_attachment[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_attachment[0].alphaBlendOp = VK_BLEND_OP_ADD;
    color_attachment[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_info = {};
    depth_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    VkPipelineColorBlendStateCreateInfo blend_info = {};
    blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_info.attachmentCount = 1;
    blend_info.pAttachments = color_attachment;

    VkDynamicState dynamic_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    // Create the graphics pipeline with dynamic rendering
    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &m_colorFormat;
    renderingInfo.depthAttachmentFormat = m_depthFormat;
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

    VkGraphicsPipelineCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.flags = 0;
    create_info.stageCount = 2;
    create_info.pStages = &shaderStages[0];
    create_info.pVertexInputState = &vertex_info;
    create_info.pInputAssemblyState = &ia_info;
    create_info.pViewportState = &viewport_info;
    create_info.pRasterizationState = &raster_info;
    create_info.pMultisampleState = &ms_info;
    create_info.pDepthStencilState = &depth_info;
    create_info.pColorBlendState = &blend_info;
    create_info.pDynamicState = &dynamic_state;
    create_info.layout = m_pipelineLayout;

    create_info.pNext = &renderingInfo;
    create_info.renderPass = VK_NULL_HANDLE; // Just make sure it's actually nullptr.

    ErrorCheck( vkCreateGraphicsPipelines(cm_device, nullptr, 1, &create_info, nullptr, &m_pipeline));
}
