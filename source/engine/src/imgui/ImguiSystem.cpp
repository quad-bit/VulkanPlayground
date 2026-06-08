#include "imgui.h"
#include "utils.h"
#ifndef IMGUI_DISABLE
#include "imgui/ImguiSystem.h"
#include "imgui/ImguiGlfwHelper.h"
#include <stdio.h>
#ifndef IM_MAX
#define IM_MAX(A, B)    (((A) >= (B)) ? (A) : (B))
#endif
#undef Status // X11 headers are leaking this.

// Visual Studio warnings
#ifdef _MSC_VER
#pragma warning (disable: 4127) // condition expression is constant
#endif

// Clang/GCC warnings with -Weverything
#if defined(__clang__)
#pragma clang diagnostic ignored "-Wold-style-cast"                 // warning: use of old-style cast
#pragma clang diagnostic ignored "-Wsign-conversion"                // warning: implicit conversion changes signedness
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"  // warning: implicit conversion from 'xxx' to 'float' may lose precision
#pragma clang diagnostic ignored "-Wcast-function-type"             // warning: cast between incompatible function types (for loader)
#endif

// Forward Declarations
struct ImGuiFrameRenderBuffers;
struct ImGuiWindowRenderBuffers;
bool ImGuiCreateDeviceObjects();
void ImGuiDestroyDeviceObjects();
void ImGuiDestroyFrameRenderBuffers(VkDevice device, ImGuiFrameRenderBuffers* buffers, const VkAllocationCallbacks* allocator);
void ImGuiDestroyWindowRenderBuffers(VkDevice device, ImGuiWindowRenderBuffers* buffers, const VkAllocationCallbacks* allocator);
//void ImGuiDestroyFrame(VkDevice device, ImGuiFrame* fd, const VkAllocationCallbacks* allocator);
//void ImGuiDestroyFrameSemaphores(VkDevice device, ImGuiFrameSemaphores* fsd, const VkAllocationCallbacks* allocator);
//void ImGuiCreateWindowSwapChain(VkPhysicalDevice physical_device, VkDevice device, ImGuiWindow* wd, const VkAllocationCallbacks* allocator, int w, int h, uint32_t minImageCount, VkImageUsageFlags imageUsage);
//void ImGuiCreateWindowCommandBuffers(VkPhysicalDevice physical_device, VkDevice device, ImGuiWindow* wd, uint32_t queueFamily, const VkAllocationCallbacks* allocator);

GLFWwindow* Loops::ImguiSystem::m_mouseWindow = nullptr;
ImVec2       Loops::ImguiSystem::m_lastValidMousePos = ImVec2(0, 0);

struct ImGuiFrameRenderBuffers
{
    VkDeviceMemory      m_vertexBufferMemory = VK_NULL_HANDLE;
    VkDeviceMemory      m_indexBufferMemory = VK_NULL_HANDLE;
    VkDeviceSize        m_vertexBufferSize = 0;
    VkDeviceSize        m_indexBufferSize = 0;
    VkBuffer            m_vertexBuffer = VK_NULL_HANDLE;
    VkBuffer            m_indexBuffer = VK_NULL_HANDLE;
};

// Each viewport will hold 1 ImGui_ImplVulkanH_WindowRenderBuffers
// [Please zero-clear before use!]
struct ImGuiWindowRenderBuffers
{
    uint32_t            m_index = 0;
    uint32_t            m_count = 0;
    ImVector<ImGuiFrameRenderBuffers> m_frameRenderBuffers;
};

struct ImGuiTexture
{
    VkDeviceMemory              m_memory = VK_NULL_HANDLE;
    VkImage                     m_image = VK_NULL_HANDLE;
    VkImageView                 m_imageView = VK_NULL_HANDLE;
    VkDescriptorSet             m_descriptorSet = VK_NULL_HANDLE;

    ImGuiTexture() { memset((void*)this, 0, sizeof(*this)); }
};

// Vulkan data
struct ImGuiData
{
    Loops::ImGuiInitInfo               m_vulkanInitInfo{};
    Loops::ImGuiRenderState*           m_renderState = nullptr;          // == ImGui::GetPlatformIO().Renderer_RenderState during rendering.
    VkDeviceSize                m_bufferMemoryAlignment = 0;
    VkDeviceSize                m_nonCoherentAtomSize = 0;
    VkPipelineCreateFlags       m_pipelineCreateFlags;
    VkDescriptorSetLayout       m_descriptorSetLayoutTexture = VK_NULL_HANDLE;
    VkDescriptorSetLayout       m_descriptorSetLayoutSampler = VK_NULL_HANDLE;
    VkPipelineLayout            m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline                  m_pipeline = VK_NULL_HANDLE;               // pipeline for main render pass (created by app)
    VkShaderModule              m_shaderModuleVert = VK_NULL_HANDLE;
    VkShaderModule              m_shaderModuleFrag = VK_NULL_HANDLE;
    VkDescriptorPool            m_descriptorPool = VK_NULL_HANDLE;
    ImVector<VkFormat>          m_pipelineRenderingCreateInfoColorAttachmentFormats; // Deep copy of format array

    // Texture management
    VkSampler                   m_samplerLinear = VK_NULL_HANDLE;
    VkSampler                   m_samplerNearest = VK_NULL_HANDLE;
    VkDescriptorSet             m_samplerLinearDS = VK_NULL_HANDLE;
    VkDescriptorSet             m_samplerNearestDS = VK_NULL_HANDLE;
    VkCommandPool               m_texCommandPool = VK_NULL_HANDLE;
    VkCommandBuffer             m_texCommandBuffer = VK_NULL_HANDLE;

    // Render buffers for main window
    ImGuiWindowRenderBuffers m_mainWindowRenderBuffers;

    ImGuiData()
    {
        memset((void*)this, 0, sizeof(*this));
        m_bufferMemoryAlignment = 256;
        m_nonCoherentAtomSize = 64;
    }
};

//-----------------------------------------------------------------------------
// SHADERS
//-----------------------------------------------------------------------------

// backends/vulkan/glsl_shader.vert, compiled with:
// # glslangValidator -V -x -o glsl_shader.vert.u32 glsl_shader.vert
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
    0x07230203,0x00010000,0x0008000b,0x0000002e,0x00000000,0x00020011,0x00000001,0x0006000b,
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
    0x0000001e,0x00000001,0x00030047,0x00000019,0x00000002,0x00050048,0x00000019,0x00000000,
    0x0000000b,0x00000000,0x00040047,0x0000001c,0x0000001e,0x00000000,0x00030047,0x0000001e,
    0x00000002,0x00050048,0x0000001e,0x00000000,0x00000023,0x00000000,0x00050048,0x0000001e,
    0x00000001,0x00000023,0x00000008,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,
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
layout(set=0, binding=0) uniform texture2D _Texture;
layout(set=1, binding=0) uniform sampler _Sampler;
layout(location = 0) in struct { vec4 Color; vec2 UV; } In;
void main()
{
    fColor = In.Color * texture(sampler2D(_Texture, _Sampler), In.UV.st);
}
*/
static uint32_t __glsl_shader_frag_spv[] =
{
    0x07230203,0x00010000,0x0008000b,0x00000023,0x00000000,0x00020011,0x00000001,0x0006000b,
    0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
    0x0007000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000d,0x00030010,
    0x00000004,0x00000007,0x00030003,0x00000002,0x000001c2,0x00040005,0x00000004,0x6e69616d,
    0x00000000,0x00040005,0x00000009,0x6c6f4366,0x0000726f,0x00030005,0x0000000b,0x00000000,
    0x00050006,0x0000000b,0x00000000,0x6f6c6f43,0x00000072,0x00040006,0x0000000b,0x00000001,
    0x00005655,0x00030005,0x0000000d,0x00006e49,0x00050005,0x00000015,0x7865545f,0x65727574,
    0x00000000,0x00050005,0x00000019,0x6d61535f,0x72656c70,0x00000000,0x00040047,0x00000009,
    0x0000001e,0x00000000,0x00040047,0x0000000d,0x0000001e,0x00000000,0x00040047,0x00000015,
    0x00000021,0x00000000,0x00040047,0x00000015,0x00000022,0x00000000,0x00040047,0x00000019,
    0x00000021,0x00000000,0x00040047,0x00000019,0x00000022,0x00000001,0x00020013,0x00000002,
    0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,
    0x00000006,0x00000004,0x00040020,0x00000008,0x00000003,0x00000007,0x0004003b,0x00000008,
    0x00000009,0x00000003,0x00040017,0x0000000a,0x00000006,0x00000002,0x0004001e,0x0000000b,
    0x00000007,0x0000000a,0x00040020,0x0000000c,0x00000001,0x0000000b,0x0004003b,0x0000000c,
    0x0000000d,0x00000001,0x00040015,0x0000000e,0x00000020,0x00000001,0x0004002b,0x0000000e,
    0x0000000f,0x00000000,0x00040020,0x00000010,0x00000001,0x00000007,0x00090019,0x00000013,
    0x00000006,0x00000001,0x00000000,0x00000000,0x00000000,0x00000001,0x00000000,0x00040020,
    0x00000014,0x00000000,0x00000013,0x0004003b,0x00000014,0x00000015,0x00000000,0x0002001a,
    0x00000017,0x00040020,0x00000018,0x00000000,0x00000017,0x0004003b,0x00000018,0x00000019,
    0x00000000,0x0003001b,0x0000001b,0x00000013,0x0004002b,0x0000000e,0x0000001d,0x00000001,
    0x00040020,0x0000001e,0x00000001,0x0000000a,0x00050036,0x00000002,0x00000004,0x00000000,
    0x00000003,0x000200f8,0x00000005,0x00050041,0x00000010,0x00000011,0x0000000d,0x0000000f,
    0x0004003d,0x00000007,0x00000012,0x00000011,0x0004003d,0x00000013,0x00000016,0x00000015,
    0x0004003d,0x00000017,0x0000001a,0x00000019,0x00050056,0x0000001b,0x0000001c,0x00000016,
    0x0000001a,0x00050041,0x0000001e,0x0000001f,0x0000000d,0x0000001d,0x0004003d,0x0000000a,
    0x00000020,0x0000001f,0x00050057,0x00000007,0x00000021,0x0000001c,0x00000020,0x00050085,
    0x00000007,0x00000022,0x00000012,0x00000021,0x0003003e,0x00000009,0x00000022,0x000100fd,
    0x00010038
};

//-----------------------------------------------------------------------------
// FUNCTIONS
//-----------------------------------------------------------------------------

//#ifdef IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
//
//static PFN_vkCmdBeginRenderingKHR   ImGuiImplVulkanFuncs_vkCmdBeginRenderingKHR;
//static PFN_vkCmdEndRenderingKHR     ImGuiImplVulkanFuncs_vkCmdEndRenderingKHR;
//
//static void ImGui_ImplVulkan_LoadDynamicRenderingFunctions(uint32_t api_version, PFN_vkVoidFunction(*loader_func)(const char* function_name, void* user_data), void* user_data)
//{
//    IM_UNUSED(api_version);
//
//    // Manually load those two (see #5446, #8326, #8365, #8600)
//    // - Try loading core (non-KHR) versions first (this will work for Vulkan 1.3+ and the device supports dynamic rendering)
//    ImGuiImplVulkanFuncs_vkCmdBeginRenderingKHR = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(loader_func("vkCmdBeginRendering", user_data));
//    ImGuiImplVulkanFuncs_vkCmdEndRenderingKHR = reinterpret_cast<PFN_vkCmdEndRenderingKHR>(loader_func("vkCmdEndRendering", user_data));
//
//    // - Fallback to KHR versions if core not available (this will work if KHR extension is available and enabled and also the device supports dynamic rendering)
//    if (ImGuiImplVulkanFuncs_vkCmdBeginRenderingKHR == nullptr || ImGuiImplVulkanFuncs_vkCmdEndRenderingKHR == nullptr)
//    {
//        ImGuiImplVulkanFuncs_vkCmdBeginRenderingKHR = reinterpret_cast<PFN_vkCmdBeginRenderingKHR>(loader_func("vkCmdBeginRenderingKHR", user_data));
//        ImGuiImplVulkanFuncs_vkCmdEndRenderingKHR = reinterpret_cast<PFN_vkCmdEndRenderingKHR>(loader_func("vkCmdEndRenderingKHR", user_data));
//    }
//}
//#endif

// Backend data stored in io.BackendRendererUserData to allow support for multiple Dear ImGui contexts
// It is STRONGLY preferred that you use docking branch with multi-viewports (== single Dear ImGui context + multiple windows) instead of multiple Dear ImGui contexts.
// FIXME: multi-context support is not tested and probably dysfunctional in this backend.
static ImGuiData* ImGuiGetBackendData()
{
    return ImGui::GetCurrentContext() ? (ImGuiData*)ImGui::GetIO().BackendRendererUserData : nullptr;
}

static uint32_t ImGuiMemoryType(VkMemoryPropertyFlags properties, uint32_t type_bits)
{
    ImGuiData* bd = ImGuiGetBackendData();
    Loops::ImGuiInitInfo* v = &bd->m_vulkanInitInfo;
    VkPhysicalDeviceMemoryProperties prop;
    vkGetPhysicalDeviceMemoryProperties(v->m_physicalDevice, &prop);
    for (uint32_t i = 0; i < prop.memoryTypeCount; i++)
        if ((prop.memoryTypes[i].propertyFlags & properties) == properties && type_bits & (1 << i))
            return i;
    return 0xFFFFFFFF; // Unable to find memoryType
}

// Same as IM_MEMALIGN(). 'alignment' must be a power of two.
static inline VkDeviceSize AlignBufferSize(VkDeviceSize size, VkDeviceSize alignment)
{
    return (size + alignment - 1) & ~(alignment - 1);
}

static void CreateOrResizeBuffer(VkBuffer& buffer, VkDeviceMemory& buffer_memory, VkDeviceSize& buffer_size, VkDeviceSize new_size, VkBufferUsageFlagBits usage)
{
    ImGuiData* bd = ImGuiGetBackendData();
    Loops::ImGuiInitInfo* v = &bd->m_vulkanInitInfo;
    VkResult err;
    if (buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(v->m_device, buffer, v->m_allocator);
    if (buffer_memory != VK_NULL_HANDLE)
        vkFreeMemory(v->m_device, buffer_memory, v->m_allocator);

    VkDeviceSize buffer_size_aligned = AlignBufferSize(IM_MAX(v->m_minAllocationSize, new_size), bd->m_bufferMemoryAlignment);
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = buffer_size_aligned;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    Loops::VkUtils::ErrorCheck(vkCreateBuffer(v->m_device, &buffer_info, v->m_allocator, &buffer));

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(v->m_device, buffer, &req);
    bd->m_bufferMemoryAlignment = (bd->m_bufferMemoryAlignment > req.alignment) ? bd->m_bufferMemoryAlignment : req.alignment;
    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = req.size;
    alloc_info.memoryTypeIndex = ImGuiMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
    Loops::VkUtils::ErrorCheck(vkAllocateMemory(v->m_device, &alloc_info, v->m_allocator, &buffer_memory));

    Loops::VkUtils::ErrorCheck(vkBindBufferMemory(v->m_device, buffer, buffer_memory, 0));
    buffer_size = buffer_size_aligned;
}

static void ImGuiSetupRenderState(ImDrawData* draw_data, VkPipeline pipeline, VkCommandBuffer command_buffer, ImGuiFrameRenderBuffers* rb, int fb_width, int fb_height)
{
    ImGuiData* bd = ImGuiGetBackendData();

    // Bind pipeline:
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind Vertex And Index Buffer:
    if (draw_data->TotalVtxCount > 0)
    {
        VkBuffer vertex_buffers[1] = { rb->m_vertexBuffer };
        VkDeviceSize vertex_offset[1] = { 0 };
        vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, vertex_offset);
        vkCmdBindIndexBuffer(command_buffer, rb->m_indexBuffer, 0, sizeof(ImDrawIdx) == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
    }

    // Setup viewport:
    VkViewport viewport;
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (float)fb_width;
    viewport.height = (float)fb_height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    // Setup scale and translation:
    // Our visible imgui space lies from draw_data->DisplayPps (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    float constants[4];
    constants[0] = 2.0f / draw_data->DisplaySize.x; // Scale
    constants[1] = 2.0f / draw_data->DisplaySize.y;
    constants[2] = -1.0f - draw_data->DisplayPos.x * constants[0]; // Translate
    constants[3] = -1.0f - draw_data->DisplayPos.y * constants[1];
    vkCmdPushConstants(command_buffer, bd->m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 4, constants);

    // Setup sampler
    vkCmdBindDescriptorSets(bd->m_renderState->m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bd->m_pipelineLayout, 1, 1, &bd->m_samplerLinearDS, 0, nullptr);
}

// Draw callbacks
static void ImGuiDrawCallback_ResetRenderState(const ImDrawList*, const ImDrawCmd*)
{
    // Intentionally empty. Used as an identifier for rendering loop to call its code. Simpler to implement this way.
}

static void ImGuiDrawCallback_SetSamplerLinear(const ImDrawList*, const ImDrawCmd*)
{
    ImGuiData* bd = ImGuiGetBackendData();
    vkCmdBindDescriptorSets(bd->m_renderState->m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bd->m_pipelineLayout, 1, 1, &bd->m_samplerLinearDS, 0, nullptr); 
}

static void ImGuiDrawCallback_SetSamplerNearest(const ImDrawList*, const ImDrawCmd*)
{
    ImGuiData* bd = ImGuiGetBackendData();
    vkCmdBindDescriptorSets(bd->m_renderState->m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bd->m_pipelineLayout, 1, 1, &bd->m_samplerNearestDS, 0, nullptr); 
}

// Render function
void Loops::ImguiSystem::ImGuiRenderDrawData(ImDrawData* draw_data, VkCommandBuffer command_buffer, VkPipeline pipeline)
{
    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;

    // Catch up with texture updates. Most of the times, the list will have 1 element with an OK status, aka nothing to do.
    // (This almost always points to ImGui::GetPlatformIO().Textures[] but is part of ImDrawData to allow overriding or disabling texture updates).
    if (draw_data->Textures != nullptr)
        for (ImTextureData* tex : *draw_data->Textures)
            if (tex->Status != ImTextureStatus_OK)
                ImGuiUpdateTexture(tex);

    ImGuiData* bd = ImGuiGetBackendData();
    Loops::ImGuiInitInfo* v = &bd->m_vulkanInitInfo;
    if (pipeline == VK_NULL_HANDLE)
        pipeline = bd->m_pipeline;

    // Allocate array to store enough vertex/index buffers
    ImGuiWindowRenderBuffers* wrb = &bd->m_mainWindowRenderBuffers;
    if (wrb->m_frameRenderBuffers.Size == 0)
    {
        wrb->m_index = 0;
        wrb->m_count = v->m_imageCount;
        wrb->m_frameRenderBuffers.resize(wrb->m_count);
        memset((void*)wrb->m_frameRenderBuffers.Data, 0, wrb->m_frameRenderBuffers.size_in_bytes());
    }
    IM_ASSERT(wrb->m_count == v->m_imageCount);
    wrb->m_index = (wrb->m_index + 1) % wrb->m_count;
    ImGuiFrameRenderBuffers* rb = &wrb->m_frameRenderBuffers[wrb->m_index];

    if (draw_data->TotalVtxCount > 0)
    {
        // Create or resize the vertex/index buffers
        VkDeviceSize vertex_size = AlignBufferSize(draw_data->TotalVtxCount * sizeof(ImDrawVert), bd->m_bufferMemoryAlignment);
        VkDeviceSize index_size = AlignBufferSize(draw_data->TotalIdxCount * sizeof(ImDrawIdx), bd->m_bufferMemoryAlignment);
        if (rb->m_vertexBuffer == VK_NULL_HANDLE || rb->m_vertexBufferSize < vertex_size)
            CreateOrResizeBuffer(rb->m_vertexBuffer, rb->m_vertexBufferMemory, rb->m_vertexBufferSize, vertex_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        if (rb->m_indexBuffer == VK_NULL_HANDLE || rb->m_indexBufferSize < index_size)
            CreateOrResizeBuffer(rb->m_indexBuffer, rb->m_indexBufferMemory, rb->m_indexBufferSize, index_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

        // Upload vertex/index data into a single contiguous GPU buffer
        ImDrawVert* vtx_dst = nullptr;
        ImDrawIdx* idx_dst = nullptr;
        Loops::VkUtils::ErrorCheck(vkMapMemory(v->m_device, rb->m_vertexBufferMemory, 0, vertex_size, 0, (void**)&vtx_dst));
        Loops::VkUtils::ErrorCheck(vkMapMemory(v->m_device, rb->m_indexBufferMemory, 0, index_size, 0, (void**)&idx_dst));
        for (const ImDrawList* draw_list : draw_data->CmdLists)
        {
            memcpy(vtx_dst, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtx_dst += draw_list->VtxBuffer.Size;
            idx_dst += draw_list->IdxBuffer.Size;
        }
        VkMappedMemoryRange range[2] = {};
        range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range[0].memory = rb->m_vertexBufferMemory;
        range[0].size = VK_WHOLE_SIZE;
        range[1].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        range[1].memory = rb->m_indexBufferMemory;
        range[1].size = VK_WHOLE_SIZE;
        Loops::VkUtils::ErrorCheck(vkFlushMappedMemoryRanges(v->m_device, 2, range));
        vkUnmapMemory(v->m_device, rb->m_vertexBufferMemory);
        vkUnmapMemory(v->m_device, rb->m_indexBufferMemory);
    }

    // Setup render state structure (for callbacks and custom texture bindings)
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    Loops::ImGuiRenderState render_state;
    render_state.m_commandBuffer = command_buffer;
    render_state.m_pipeline = pipeline;
    render_state.m_pipelineLayout = bd->m_pipelineLayout;
    platform_io.Renderer_RenderState = bd->m_renderState = &render_state;

    // Setup desired Vulkan state
    ImGuiSetupRenderState(draw_data, pipeline, command_buffer, rb, fb_width, fb_height);

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    VkDescriptorSet last_image_view = VK_NULL_HANDLE;
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    for (const ImDrawList* draw_list : draw_data->CmdLists)
    {
        for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &draw_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr)
            {
                // User callback, registered via ImDrawList::AddCallback()
                if (pcmd->UserCallback == ImGuiDrawCallback_ResetRenderState)
                {
                    ImGuiSetupRenderState(draw_data, pipeline, command_buffer, rb, fb_width, fb_height);
                    last_image_view = VK_NULL_HANDLE;
                }
                else
                    pcmd->UserCallback(draw_list, pcmd);
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

                // Clamp to viewport as vkCmdSetScissor() won't accept values that are off bounds
                if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
                if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
                if (clip_max.x > (float)fb_width) { clip_max.x = (float)fb_width; }
                if (clip_max.y > (float)fb_height) { clip_max.y = (float)fb_height; }
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle
                VkRect2D scissor;
                scissor.offset.x = (int32_t)(clip_min.x);
                scissor.offset.y = (int32_t)(clip_min.y);
                scissor.extent.width = (uint32_t)(clip_max.x - clip_min.x);
                scissor.extent.height = (uint32_t)(clip_max.y - clip_min.y);
                vkCmdSetScissor(command_buffer, 0, 1, &scissor);

                // Bind DescriptorSets for image view (font or user texture) and samplers
                VkDescriptorSet image_view = (VkDescriptorSet)pcmd->GetTexID();
                if (image_view != last_image_view)
                    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bd->m_pipelineLayout, 0, 1, &image_view, 0, nullptr);
                last_image_view = image_view;

                // Draw
                vkCmdDrawIndexed(command_buffer, pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset, pcmd->VtxOffset + global_vtx_offset, 0);
            }
        }
        global_idx_offset += draw_list->IdxBuffer.Size;
        global_vtx_offset += draw_list->VtxBuffer.Size;
    }
    platform_io.Renderer_RenderState = bd->m_renderState = nullptr;

    // Note: at this point both vkCmdSetViewport() and vkCmdSetScissor() have been called.
    // Our last values will leak into user/application rendering IF:
    // - Your app uses a pipeline with VK_DYNAMIC_STATE_VIEWPORT or VK_DYNAMIC_STATE_SCISSOR dynamic state
    // - And you forgot to call vkCmdSetViewport() and vkCmdSetScissor() yourself to explicitly set that state.
    // If you use VK_DYNAMIC_STATE_VIEWPORT or VK_DYNAMIC_STATE_SCISSOR you are responsible for setting the values before rendering.
    // In theory we should aim to backup/restore those values but I am not sure this is possible.
    // We perform a call to vkCmdSetScissor() to set back a full viewport which is likely to fix things for 99% users but technically this is not perfect. (See github #4644)
    VkRect2D scissor = { { 0, 0 }, { (uint32_t)fb_width, (uint32_t)fb_height } };
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);
}

void Loops::ImguiSystem::ImGuiDestroyTexture(ImTextureData* tex)
{
    if (ImGuiTexture* backend_tex = (ImGuiTexture*)tex->BackendUserData)
    {
        IM_ASSERT(backend_tex->m_descriptorSet == (VkDescriptorSet)tex->TexID);
        ImGuiData* bd = ImGuiGetBackendData();
        Loops::ImGuiInitInfo* v = &bd->m_vulkanInitInfo;
        Loops::ImguiSystem::ImGuiRemoveTexture(backend_tex->m_descriptorSet);

        vkDestroyImageView(v->m_device, backend_tex->m_imageView, v->m_allocator);
        vkDestroyImage(v->m_device, backend_tex->m_image, v->m_allocator);
        vkFreeMemory(v->m_device, backend_tex->m_memory, v->m_allocator);
        IM_DELETE(backend_tex);

        // Clear identifiers and mark as destroyed (in order to allow e.g. calling InvalidateDeviceObjects while running)
        tex->SetTexID(ImTextureID_Invalid);
        tex->BackendUserData = nullptr;
    }
    tex->SetStatus(ImTextureStatus_Destroyed);
}

void Loops::ImguiSystem::ImGuiUpdateTexture(ImTextureData* tex)
{
    if (tex->Status == ImTextureStatus_OK)
        return;
    ImGuiData* bd = ImGuiGetBackendData();
    Loops::ImGuiInitInfo* v = &bd->m_vulkanInitInfo;
    VkResult err;

    if (tex->Status == ImTextureStatus_WantCreate)
    {
        // Create and upload new texture to graphics system
        //IMGUI_DEBUG_LOG("UpdateTexture #%03d: WantCreate %dx%d\n", tex->UniqueID, tex->Width, tex->Height);
        IM_ASSERT(tex->TexID == ImTextureID_Invalid && tex->BackendUserData == nullptr);
        IM_ASSERT(tex->Format == ImTextureFormat_RGBA32);
        ImGuiTexture* backend_tex = IM_NEW(ImGuiTexture)();

        // Create the Image:
        {
            VkImageCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = VK_FORMAT_R8G8B8A8_UNORM;
            info.extent.width = tex->Width;
            info.extent.height = tex->Height;
            info.extent.depth = 1;
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            err = vkCreateImage(v->m_device, &info, v->m_allocator, &backend_tex->m_image);
            Loops::VkUtils::ErrorCheck(err);
            VkMemoryRequirements req;
            vkGetImageMemoryRequirements(v->m_device, backend_tex->m_image, &req);
            VkMemoryAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc_info.allocationSize = IM_MAX(v->m_minAllocationSize, req.size);
            alloc_info.memoryTypeIndex = ImGuiMemoryType(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, req.memoryTypeBits);
            err = vkAllocateMemory(v->m_device, &alloc_info, v->m_allocator, &backend_tex->m_memory);
            Loops::VkUtils::ErrorCheck(err);
            err = vkBindImageMemory(v->m_device, backend_tex->m_image, backend_tex->m_memory, 0);
            Loops::VkUtils::ErrorCheck(err);
        }

        // Create the Image View:
        {
            VkImageViewCreateInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.image = backend_tex->m_image;
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            info.format = VK_FORMAT_R8G8B8A8_UNORM;
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.subresourceRange.levelCount = 1;
            info.subresourceRange.layerCount = 1;
            err = vkCreateImageView(v->m_device, &info, v->m_allocator, &backend_tex->m_imageView);
            Loops::VkUtils::ErrorCheck(err);
        }

        // Create the Descriptor Set
        backend_tex->m_descriptorSet = ImGuiAddTexture(backend_tex->m_imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // Store identifiers
        tex->SetTexID((ImTextureID)backend_tex->m_descriptorSet);
        tex->BackendUserData = backend_tex;
    }

    if (tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates)
    {
        ImGuiTexture* backend_tex = (ImGuiTexture*)tex->BackendUserData;

        // Update full texture or selected blocks. We only ever write to textures regions which have never been used before!
        // This backend choose to use tex->UpdateRect but you can use tex->Updates[] to upload individual regions.
        // We could use the smaller rect on _WantCreate but using the full rect allows us to clear the texture.
        const int upload_x = (tex->Status == ImTextureStatus_WantCreate) ? 0 : tex->UpdateRect.x;
        const int upload_y = (tex->Status == ImTextureStatus_WantCreate) ? 0 : tex->UpdateRect.y;
        const int upload_w = (tex->Status == ImTextureStatus_WantCreate) ? tex->Width : tex->UpdateRect.w;
        const int upload_h = (tex->Status == ImTextureStatus_WantCreate) ? tex->Height : tex->UpdateRect.h;

        // Create the Upload Buffer:
        VkDeviceMemory upload_buffer_memory;

        VkBuffer upload_buffer;
        VkDeviceSize upload_pitch = upload_w * tex->BytesPerPixel;
        VkDeviceSize upload_size = AlignBufferSize(upload_h * upload_pitch, bd->m_nonCoherentAtomSize);
        {
            VkBufferCreateInfo buffer_info = {};
            buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            buffer_info.size = upload_size;
            buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            err = vkCreateBuffer(v->m_device, &buffer_info, v->m_allocator, &upload_buffer);
            Loops::VkUtils::ErrorCheck(err);
            VkMemoryRequirements req;
            vkGetBufferMemoryRequirements(v->m_device, upload_buffer, &req);
            bd->m_bufferMemoryAlignment = (bd->m_bufferMemoryAlignment > req.alignment) ? bd->m_bufferMemoryAlignment : req.alignment;
            VkMemoryAllocateInfo alloc_info = {};
            alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            alloc_info.allocationSize = IM_MAX(v->m_minAllocationSize, req.size);
            alloc_info.memoryTypeIndex = ImGuiMemoryType(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, req.memoryTypeBits);
            err = vkAllocateMemory(v->m_device, &alloc_info, v->m_allocator, &upload_buffer_memory);
            Loops::VkUtils::ErrorCheck(err);
            err = vkBindBufferMemory(v->m_device, upload_buffer, upload_buffer_memory, 0);
            Loops::VkUtils::ErrorCheck(err);
        }

        // Upload to Buffer:
        {
            char* map = nullptr;
            err = vkMapMemory(v->m_device, upload_buffer_memory, 0, upload_size, 0, (void**)(&map));
            Loops::VkUtils::ErrorCheck(err);
            for (int y = 0; y < upload_h; y++)
                memcpy(map + upload_pitch * y, tex->GetPixelsAt(upload_x, upload_y + y), (size_t)upload_pitch);
            VkMappedMemoryRange range[1] = {};
            range[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            range[0].memory = upload_buffer_memory;
            range[0].size = upload_size;
            err = vkFlushMappedMemoryRanges(v->m_device, 1, range);
            Loops::VkUtils::ErrorCheck(err);
            vkUnmapMemory(v->m_device, upload_buffer_memory);
        }

        // Start command buffer
        {
            err = vkResetCommandPool(v->m_device, bd->m_texCommandPool, 0);
            Loops::VkUtils::ErrorCheck(err);
            VkCommandBufferBeginInfo begin_info = {};
            begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin_info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            err = vkBeginCommandBuffer(bd->m_texCommandBuffer, &begin_info);
            Loops::VkUtils::ErrorCheck(err);
        }

        // Copy to Image:
        {
            VkBufferMemoryBarrier upload_barrier[1] = {};
            upload_barrier[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            upload_barrier[0].srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
            upload_barrier[0].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            upload_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            upload_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            upload_barrier[0].buffer = upload_buffer;
            upload_barrier[0].offset = 0;
            upload_barrier[0].size = upload_size;

            VkImageMemoryBarrier copy_barrier[1] = {};
            copy_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            copy_barrier[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            copy_barrier[0].oldLayout = (tex->Status == ImTextureStatus_WantCreate) ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            copy_barrier[0].newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            copy_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            copy_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            copy_barrier[0].image = backend_tex->m_image;
            copy_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy_barrier[0].subresourceRange.levelCount = 1;
            copy_barrier[0].subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(bd->m_texCommandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, upload_barrier, 1, copy_barrier);

            VkBufferImageCopy region = {};
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.layerCount = 1;
            region.imageExtent.width = upload_w;
            region.imageExtent.height = upload_h;
            region.imageExtent.depth = 1;
            region.imageOffset.x = upload_x;
            region.imageOffset.y = upload_y;
            vkCmdCopyBufferToImage(bd->m_texCommandBuffer, upload_buffer, backend_tex->m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

            VkImageMemoryBarrier use_barrier[1] = {};
            use_barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            use_barrier[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            use_barrier[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            use_barrier[0].oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            use_barrier[0].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            use_barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            use_barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            use_barrier[0].image = backend_tex->m_image;
            use_barrier[0].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            use_barrier[0].subresourceRange.levelCount = 1;
            use_barrier[0].subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(bd->m_texCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, use_barrier);
        }

        // End command buffer
        {
            VkSubmitInfo end_info = {};
            end_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            end_info.commandBufferCount = 1;
            end_info.pCommandBuffers = &bd->m_texCommandBuffer;
            err = vkEndCommandBuffer(bd->m_texCommandBuffer);
            Loops::VkUtils::ErrorCheck(err);
            err = vkQueueSubmit(v->m_queue, 1, &end_info, VK_NULL_HANDLE);
            Loops::VkUtils::ErrorCheck(err);
        }

        err = vkQueueWaitIdle(v->m_queue); // FIXME-OPT: Suboptimal!
        Loops::VkUtils::ErrorCheck(err);
        vkDestroyBuffer(v->m_device, upload_buffer, v->m_allocator);
        vkFreeMemory(v->m_device, upload_buffer_memory, v->m_allocator);

        tex->SetStatus(ImTextureStatus_OK);
    }

    if (tex->Status == ImTextureStatus_WantDestroy && tex->UnusedFrames >= (int)bd->m_vulkanInitInfo.m_imageCount)
    {
        //ImGuiDestroyTexture(tex);
        if (ImGuiTexture* backend_tex = (ImGuiTexture*)tex->BackendUserData)
        {
            IM_ASSERT(backend_tex->m_descriptorSet == (VkDescriptorSet)tex->TexID);
            ImGuiData* bd = ImGuiGetBackendData();
            Loops::ImGuiInitInfo* v = &bd->m_vulkanInitInfo;
            ImGuiRemoveTexture(backend_tex->m_descriptorSet);
            vkDestroyImageView(v->m_device, backend_tex->m_imageView, v->m_allocator);
            vkDestroyImage(v->m_device, backend_tex->m_image, v->m_allocator);
            vkFreeMemory(v->m_device, backend_tex->m_memory, v->m_allocator);
            IM_DELETE(backend_tex);

            // Clear identifiers and mark as destroyed (in order to allow e.g. calling InvalidateDeviceObjects while running)
            tex->SetTexID(ImTextureID_Invalid);
            tex->BackendUserData = nullptr;
        }
        tex->SetStatus(ImTextureStatus_Destroyed);
    }
}


static void ImGuiCreateShaderModules(VkDevice device, const VkAllocationCallbacks* allocator)
{
    ImGuiData* bd = ImGuiGetBackendData();
    Loops::ImGuiInitInfo* v = &bd->m_vulkanInitInfo;
    if (bd->m_shaderModuleVert == VK_NULL_HANDLE)
    {
        VkShaderModuleCreateInfo default_vert_info = {};
        default_vert_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        default_vert_info.codeSize = sizeof(__glsl_shader_vert_spv);
        default_vert_info.pCode = (uint32_t*)__glsl_shader_vert_spv;
        VkShaderModuleCreateInfo* p_vert_info = (v->m_customShaderVertCreateInfo.sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO) ? &v->m_customShaderVertCreateInfo : &default_vert_info;
        VkResult err = vkCreateShaderModule(device, p_vert_info, allocator, &bd->m_shaderModuleVert);
        Loops::VkUtils::ErrorCheck(err);
    }
    if (bd->m_shaderModuleFrag == VK_NULL_HANDLE)
    {
        VkShaderModuleCreateInfo default_frag_info = {};
        default_frag_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        default_frag_info.codeSize = sizeof(__glsl_shader_frag_spv);
        default_frag_info.pCode = (uint32_t*)__glsl_shader_frag_spv;
        VkShaderModuleCreateInfo* p_frag_info = (v->m_customShaderFragCreateInfo.sType == VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO) ? &v->m_customShaderFragCreateInfo : &default_frag_info;
        VkResult err = vkCreateShaderModule(device, p_frag_info, allocator, &bd->m_shaderModuleFrag);
        Loops::VkUtils::ErrorCheck(err);
    }
}

#if !defined(IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING) && !(defined(VK_VERSION_1_3) || defined(VK_KHR_dynamic_rendering))
typedef void VkPipelineRenderingCreateInfoKHR;
#endif

static VkPipeline ImGuiCreatePipeline(VkDevice device, const VkAllocationCallbacks* allocator, VkPipelineCache pipelineCache, const VkPipelineRenderingCreateInfoKHR& info)
{
    ImGuiData* bd = ImGuiGetBackendData();
    ImGuiCreateShaderModules(device, allocator);

    VkPipelineShaderStageCreateInfo stage[2] = {};
    stage[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage[0].module = bd->m_shaderModuleVert;
    stage[0].pName = "main";
    stage[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage[1].module = bd->m_shaderModuleFrag;
    stage[1].pName = "main";

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

    ImVector<VkDynamicState> dynamic_states{};
    dynamic_states.push_back(VK_DYNAMIC_STATE_VIEWPORT);
    dynamic_states.push_back(VK_DYNAMIC_STATE_SCISSOR);
    VkPipelineDynamicStateCreateInfo dynamic_state = {};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = dynamic_states.Size;
    dynamic_state.pDynamicStates = dynamic_states.Data;

    VkGraphicsPipelineCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    create_info.flags = bd->m_pipelineCreateFlags;
    create_info.stageCount = 2;
    create_info.pStages = stage;
    create_info.pVertexInputState = &vertex_info;
    create_info.pInputAssemblyState = &ia_info;
    create_info.pViewportState = &viewport_info;
    create_info.pRasterizationState = &raster_info;
    create_info.pMultisampleState = &ms_info;
    create_info.pDepthStencilState = &depth_info;
    create_info.pColorBlendState = &blend_info;
    create_info.pDynamicState = &dynamic_state;
    create_info.layout = bd->m_pipelineLayout;
    //create_info.renderPass = info->RenderPass;
    //create_info.subpass = info->Subpass;

#ifdef IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
    if (bd->m_vulkanInitInfo.m_useDynamicRendering)
    {
        IM_ASSERT(info.sType == VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR && "PipelineRenderingCreateInfo::sType must be VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR");
        IM_ASSERT(info.pNext == nullptr && "PipelineRenderingCreateInfo::pNext must be nullptr");
        create_info.pNext = &info;
        create_info.renderPass = VK_NULL_HANDLE; // Just make sure it's actually nullptr.
    }
#endif
    VkPipeline pipeline;
    VkResult err = vkCreateGraphicsPipelines(device, pipelineCache, 1, &create_info, allocator, &pipeline);
    Loops::VkUtils::ErrorCheck(err);
    return pipeline;
}

static void ImGui_ImplVulkan_CreateDescriptorSetLayout(VkDescriptorSetLayout* p_layout, VkDescriptorType descriptor_type)
{
    ImGuiData* bd = ImGuiGetBackendData();
    Loops::ImGuiInitInfo* v = &bd->m_vulkanInitInfo;

    VkDescriptorSetLayoutBinding binding[1] = {};
    binding[0].descriptorType = descriptor_type;
    binding[0].descriptorCount = 1;
    binding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    info.bindingCount = 1;
    info.pBindings = binding;
    VkResult err = vkCreateDescriptorSetLayout(v->m_device, &info, v->m_allocator, p_layout);
    Loops::VkUtils::ErrorCheck(err);
}

static VkDescriptorSet ImGuiCreateSamplerDS(VkSampler sampler)
{
    ImGuiData* bd = ImGuiGetBackendData();
    Loops::ImGuiInitInfo* v = &bd->m_vulkanInitInfo;

    VkDescriptorSet descriptor_set;
    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = bd->m_descriptorPool ? bd->m_descriptorPool : v->m_descriptorPool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &bd->m_descriptorSetLayoutSampler;
    VkResult err = vkAllocateDescriptorSets(v->m_device, &alloc_info, &descriptor_set);
    Loops::VkUtils::ErrorCheck(err);

    VkDescriptorImageInfo desc_image = {};
    desc_image.sampler = sampler;
    VkWriteDescriptorSet sampler_write_desc = {};
    sampler_write_desc.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    sampler_write_desc.dstSet = descriptor_set;
    sampler_write_desc.descriptorCount = 1;
    sampler_write_desc.dstBinding = 0;
    sampler_write_desc.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    sampler_write_desc.pImageInfo = &desc_image;
    vkUpdateDescriptorSets(v->m_device, 1, &sampler_write_desc, 0, nullptr);
    return descriptor_set;
}

// If unspecified by user, assume that ApiVersion == HeaderVersion
 // We don't care about other versions than 1.3 for our checks, so don't need to make this exhaustive (e.g. with all #ifdef VK_VERSION_1_X checks)
static uint32_t ImGuiGetDefaultApiVersion()
{
#ifdef VK_HEADER_VERSION_COMPLETE
    return VK_HEADER_VERSION_COMPLETE;
#else
    return VK_API_VERSION_1_0;
#endif
}

//bool    ImGui_ImplVulkan_LoadFunctions(uint32_t api_version, PFN_vkVoidFunction(*loader_func)(const char* function_name, void* user_data), void* user_data)
//{
//    // Load function pointers
//    // You can use the default Vulkan loader using:
//    //      ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_3, [](const char* function_name, void*) { return vkGetInstanceProcAddr(your_vk_isntance, function_name); });
//    // But this would be roughly equivalent to not setting VK_NO_PROTOTYPES.
//    if (api_version == 0)
//        api_version = ImGui_ImplVulkan_GetDefaultApiVersion();
//
//#ifdef IMGUI_IMPL_VULKAN_USE_LOADER
//#define IMGUI_VULKAN_FUNC_LOAD(func) \
//    func = reinterpret_cast<decltype(func)>(loader_func(#func, user_data)); \
//    if (func == nullptr)   \
//        return false;
//    IMGUI_VULKAN_FUNC_MAP(IMGUI_VULKAN_FUNC_LOAD)
//#undef IMGUI_VULKAN_FUNC_LOAD
//
//#ifdef IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
//        ImGui_ImplVulkan_LoadDynamicRenderingFunctions(api_version, loader_func, user_data);
//#endif
//#else
//    IM_UNUSED(loader_func);
//    IM_UNUSED(user_data);
//#endif
//
//    g_FunctionsLoaded = true;
//    return true;
//}


void ImGuiDestroyFrameRenderBuffers(VkDevice device, ImGuiFrameRenderBuffers* buffers, const VkAllocationCallbacks* allocator)
{
    if (buffers->m_vertexBuffer) { vkDestroyBuffer(device, buffers->m_vertexBuffer, allocator); buffers->m_vertexBuffer = VK_NULL_HANDLE; }
    if (buffers->m_vertexBufferMemory) { vkFreeMemory(device, buffers->m_vertexBufferMemory, allocator); buffers->m_vertexBufferMemory = VK_NULL_HANDLE; }
    if (buffers->m_indexBuffer) { vkDestroyBuffer(device, buffers->m_indexBuffer, allocator); buffers->m_indexBuffer = VK_NULL_HANDLE; }
    if (buffers->m_indexBufferMemory) { vkFreeMemory(device, buffers->m_indexBufferMemory, allocator); buffers->m_indexBufferMemory = VK_NULL_HANDLE; }
    buffers->m_vertexBufferSize = 0;
    buffers->m_indexBufferSize = 0;
}

void ImGuiDestroyWindowRenderBuffers(VkDevice device, ImGuiWindowRenderBuffers* buffers, const VkAllocationCallbacks* allocator)
{
    for (uint32_t n = 0; n < buffers->m_count; n++)
        ImGuiDestroyFrameRenderBuffers(device, &buffers->m_frameRenderBuffers[n], allocator);
    buffers->m_frameRenderBuffers.clear();
    buffers->m_index = 0;
    buffers->m_count = 0;
}

// ================================== Loops::ImguiSystem ============================================

//Loops::ImguiSystem::ImguiSystem(GLFWwindow* window, const Loops::VulkanManager* vulkanManager, const VkDevice& device, const VkPhysicalDevice& physicalDevice,
//    const VkQueue& graphicsQueue, uint32_t graphicsQueueFamily, uint8_t frameInFlights,
//    uint32_t frameBufferWidth, uint32_t framebufferHeight, VkFormat depthFormat, VkFormat colorFormat,
//    const std::vector<VkImageView>& colorViews
//) : m_frameBufferWidth(frameBufferWidth), m_framebufferHeight(framebufferHeight), m_colorAttachmentViews(colorViews),
//    m_surfaceColorFormat(colorFormat), m_maxFrameInFlights(frameInFlights)

Loops::ImguiSystem::ImguiSystem(GLFWwindow* window, const Loops::VulkanManager* vulkanManager, const VkDevice& device, const VkPhysicalDevice& physicalDevice,
    const VkQueue& graphicsQueue, uint32_t graphicsQueueFamily, uint8_t frameInFlights,
    uint32_t frameBufferWidth, uint32_t framebufferHeight, VkFormat colorFormat, const std::vector<VkImageView>& colorViews
) : m_frameBufferWidth(frameBufferWidth), m_framebufferHeight(framebufferHeight), m_colorAttachmentViews(colorViews),
    m_surfaceColorFormat(colorFormat), m_maxFrameInFlights(frameInFlights)
{

    //ImGuiWindow* wd = &g_MainWindowData;
    // initialises ImGui_ImplVulkanH_Window with surface and surface format
    //SetupVulkanWindow(wd, surface, w, h);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.DisplaySize = ImVec2(static_cast<float>(m_frameBufferWidth), static_cast<float>(m_framebufferHeight));
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(1.0f);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = 1.0f;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)

    //installs callback
    //ImGui_ImplGlfw_InitForVulkan(window, true);

    // 
    // Create Descriptor Pool
    // If you wish to load e.g. additional textures you may need to alter pools sizes and maxSets.
    /*{
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE },
            { VK_DESCRIPTOR_TYPE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE },
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 0;
        for (VkDescriptorPoolSize& pool_size : pool_sizes)
            pool_info.maxSets += pool_size.descriptorCount;
        pool_info.poolSizeCount = (uint32_t)IM_COUNTOF(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        Loops::VkUtils::ErrorCheck(vkCreateDescriptorPool(vulkanManager->GetLogicalDevice(), &pool_info, nullptr, &m_descriptorPool));
    }*/

    ImGuiInitInfo initInfo = {};
    //initInfo.ApiVersion = VK_API_VERSION_1_3;              // Pass in your value of VkApplicationInfo::apiVersion, otherwise will default to header version.
    initInfo.m_instance = vulkanManager->GetInstance();
    initInfo.m_physicalDevice = physicalDevice;
    initInfo.m_device = device;
    initInfo.m_queueFamily = graphicsQueueFamily;
    initInfo.m_queue = graphicsQueue;
    initInfo.m_pipelineCache = VK_NULL_HANDLE;
    initInfo.m_descriptorPool = m_descriptorPool;
    initInfo.m_minImageCount = 2;
    initInfo.m_imageCount = vulkanManager->GetSwapchainImageCount();
    initInfo.m_allocator = nullptr;

    ImGuiInit(&initInfo);

    // Load Fonts
    // - If fonts are not explicitly loaded, Dear ImGui will select an embedded font: either AddFontDefaultVector() or AddFontDefaultBitmap().
    //   This selection is based on (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) reaching a small threshold.
    // - You can load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - If a file cannot be loaded, AddFont functions will return a nullptr. Please handle those errors in your code (e.g. use an assertion, display an error and quit).
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType for higher quality font rendering.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //style.FontSizeBase = 20.0f;
    //io.Fonts->AddFontDefaultVector();
    //io.Fonts->AddFontDefaultBitmap();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);

    // create command pool and command buffer for frame render
    {
        VkCommandPoolCreateInfo createInfo{};
        createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        createInfo.queueFamilyIndex = graphicsQueueFamily;
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

        Loops::VkUtils::ErrorCheck(vkCreateCommandPool(device, &createInfo, nullptr, &m_commandPool));

        m_commandBuffers.resize(frameInFlights);
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.commandBufferCount = frameInFlights;
        alloc_info.commandPool = m_commandPool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;

        Loops::VkUtils::ErrorCheck(vkAllocateCommandBuffers(device, &alloc_info, &m_commandBuffers[0]));
    }

    /*{
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
    }*/
}

void Loops::ImguiSystem::CreateRenderingInfo()
{
    m_renderingInfoCreated = true;

    {
        m_colorList.resize(m_maxFrameInFlights);
        for (uint32_t i = 0; i < m_maxFrameInFlights; i++)
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
            info.renderArea = VkRect2D{ {0, 0}, {(uint32_t)m_frameBufferWidth, m_framebufferHeight} };
            info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            m_renderingInfoList.push_back(std::move(info));
        }
    }
}

void Loops::ImguiSystem::CreateRenderingInfo(const VkClearColorValue& clearColor)
{
    m_renderingInfoCreated = true;

    {
        m_colorList.resize(m_maxFrameInFlights);
        for (uint32_t i = 0; i < m_maxFrameInFlights; i++)
        {
            m_colorList[i].clearValue.color = clearColor;
            m_colorList[i].imageLayout = VkImageLayout::VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            m_colorList[i].imageView = m_colorAttachmentViews[i];
            m_colorList[i].loadOp = VkAttachmentLoadOp::VK_ATTACHMENT_LOAD_OP_CLEAR;
            m_colorList[i].storeOp = VkAttachmentStoreOp::VK_ATTACHMENT_STORE_OP_STORE;
            m_colorList[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;

            VkRenderingInfo info{};
            info.colorAttachmentCount = (1);
            info.layerCount = (1);
            info.pColorAttachments = &m_colorList[i];
            info.renderArea = VkRect2D{ {0, 0}, {(uint32_t)m_frameBufferWidth, m_framebufferHeight} };
            info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            m_renderingInfoList.push_back(std::move(info));
        }
    }
}


Loops::ImguiSystem::~ImguiSystem()
{
    ImGuiShutdown();
}

void Loops::ImguiSystem::ImGuiRemoveTexture(VkDescriptorSet descriptor_set)
{
    ImGuiData* bd = ImGuiGetBackendData();
    Loops::ImGuiInitInfo* v = &bd->m_vulkanInitInfo;
    VkDescriptorPool pool = bd->m_descriptorPool ? bd->m_descriptorPool : v->m_descriptorPool;
    vkFreeDescriptorSets(v->m_device, pool, 1, &descriptor_set);
}

void Loops::ImguiSystem::ImGuiNewFrame()
{
    IM_ASSERT(m_renderingInfoCreated && "CreateRenderingInfo not called after constructor and befor new frame");
    ImGuiData* bd = ImGuiGetBackendData();
    IM_ASSERT(bd != nullptr && "Context or backend not initialized! Did you call ImGui_ImplVulkan_Init()?");
    IM_UNUSED(bd);
    ImGui::NewFrame();
}

void Loops::ImguiSystem::ImGuiSetMinImageCount(uint32_t min_image_count)
{
    ImGuiData* bd = ImGuiGetBackendData();
    IM_ASSERT(min_image_count >= 2);
    if (bd->m_vulkanInitInfo.m_minImageCount == min_image_count)
        return;

    Loops::ImGuiInitInfo* v = &bd->m_vulkanInitInfo;
    VkResult err = vkDeviceWaitIdle(v->m_device);
    Loops::VkUtils::ErrorCheck(err);
    ImGuiDestroyWindowRenderBuffers(v->m_device, &bd->m_mainWindowRenderBuffers, v->m_allocator);
    bd->m_vulkanInitInfo.m_minImageCount = min_image_count;
}

// Register a texture by creating a descriptor
// FIXME: This is experimental in the sense that we are unsure how to best design/tackle this problem, please post to https://github.com/ocornut/imgui/pull/914 if you have suggestions.
VkDescriptorSet Loops::ImguiSystem::ImGuiAddTexture(VkImageView image_view, VkImageLayout image_layout)
{
    ImGuiData* bd = ImGuiGetBackendData();
    Loops::ImGuiInitInfo* v = &bd->m_vulkanInitInfo;
    VkDescriptorPool pool = bd->m_descriptorPool ? bd->m_descriptorPool : v->m_descriptorPool;

    // Create Descriptor Set:
    VkDescriptorSet descriptor_set;
    {
        VkDescriptorSetAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool = pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts = &bd->m_descriptorSetLayoutTexture;
        VkResult err = vkAllocateDescriptorSets(v->m_device, &alloc_info, &descriptor_set);
        Loops::VkUtils::ErrorCheck(err);
    }

    // Update the Descriptor Set:
    if (descriptor_set != VK_NULL_HANDLE)
    {
        VkDescriptorImageInfo desc_image[1] = {};
        desc_image[0].imageView = image_view;
        desc_image[0].imageLayout = image_layout;
        VkWriteDescriptorSet write_desc[1] = {};
        write_desc[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_desc[0].dstSet = descriptor_set;
        write_desc[0].descriptorCount = 1;
        write_desc[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write_desc[0].pImageInfo = desc_image;
        vkUpdateDescriptorSets(v->m_device, 1, write_desc, 0, nullptr);
    }
    return descriptor_set;
}

bool Loops::ImguiSystem::ImGuiInit(Loops::ImGuiInitInfo* info)
{
    //IM_ASSERT(g_FunctionsLoaded && "Need to call ImGui_ImplVulkan_LoadFunctions() if IMGUI_IMPL_VULKAN_NO_PROTOTYPES or VK_NO_PROTOTYPES are set!");

    if (info->m_apiVersion == 0)
        info->m_apiVersion = ImGuiGetDefaultApiVersion();

    if (info->m_useDynamicRendering)
    {
        //#ifdef IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
        //#ifndef IMGUI_IMPL_VULKAN_USE_LOADER
        //        ImGuiLoadDynamicRenderingFunctions(info->ApiVersion, [](const char* function_name, void* user_data) { return vkGetDeviceProcAddr((VkDevice)user_data, function_name); }, (void*)info->Device);
        //#endif
        //        IM_ASSERT(ImGuiImplVulkanFuncs_vkCmdBeginRenderingKHR != nullptr);
        //        IM_ASSERT(ImGuiImplVulkanFuncs_vkCmdEndRenderingKHR != nullptr);
        //#else
        //        IM_ASSERT(0 && "Can't use dynamic rendering when neither VK_VERSION_1_3 or VK_KHR_dynamic_rendering is defined.");
        //#endif
    }

    ImGuiIO& io = ImGui::GetIO();
    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    // Setup backend capabilities flags
    ImGuiData* bd = IM_NEW(ImGuiData)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "ImguiSystem";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;   // We can honor ImGuiPlatformIO::Textures[] requests during render.

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.DrawCallback_ResetRenderState = ImGuiDrawCallback_ResetRenderState;
    platform_io.DrawCallback_SetSamplerLinear = ImGuiDrawCallback_SetSamplerLinear;
    platform_io.DrawCallback_SetSamplerNearest = ImGuiDrawCallback_SetSamplerNearest;

    // Sanity checks
    IM_ASSERT(info->m_instance != VK_NULL_HANDLE);
    IM_ASSERT(info->m_physicalDevice != VK_NULL_HANDLE);
    IM_ASSERT(info->m_device != VK_NULL_HANDLE);
    IM_ASSERT(info->m_queue != VK_NULL_HANDLE);
    IM_ASSERT(info->m_minImageCount >= 2);
    IM_ASSERT(info->m_imageCount >= info->m_minImageCount);
    if (info->m_descriptorPool != VK_NULL_HANDLE) // Either DescriptorPool or DescriptorPoolSize must be set, not both!
        IM_ASSERT(info->m_descriptorPoolSize == 0);
    else
        IM_ASSERT(info->m_descriptorPoolSize > 0);
    //if (info->m_useDynamicRendering)
    //    IM_ASSERT(info->m_pipelineInfoMain.RenderPass == VK_NULL_HANDLE);

    bd->m_vulkanInitInfo = *info;

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(info->m_physicalDevice, &properties);
    bd->m_nonCoherentAtomSize = properties.limits.nonCoherentAtomSize;

    if (!ImGuiCreateDeviceObjects())
        IM_ASSERT(0 && "ImGui_ImplVulkan_CreateDeviceObjects() failed!"); // <- Can't be hit yet.

    if (!bd->m_pipelineLayout)
    {
        // Constants: we are using 'vec2 offset' and 'vec2 scale' instead of a full 3d projection matrix
        VkPushConstantRange push_constants[1] = {};
        push_constants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        push_constants[0].offset = sizeof(float) * 0;
        push_constants[0].size = sizeof(float) * 4;
        VkDescriptorSetLayout set_layout[2] = { bd->m_descriptorSetLayoutTexture, bd->m_descriptorSetLayoutSampler };
        VkPipelineLayoutCreateInfo layout_info = {};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 2;
        layout_info.pSetLayouts = set_layout;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = push_constants;
        Loops::VkUtils::ErrorCheck(vkCreatePipelineLayout(info->m_device, &layout_info, info->m_allocator, &bd->m_pipelineLayout));

        info->m_pipelineRenderingCreateInfo.colorAttachmentCount = 1;
        info->m_pipelineRenderingCreateInfo.pColorAttachmentFormats = &m_surfaceColorFormat;
        info->m_pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

        ImGuiCreateMainPipeline(&info->m_pipelineRenderingCreateInfo);
    }


    return true;
}

void Loops::ImguiSystem::ImGuiShutdown()
{
    ImGuiData* bd = ImGuiGetBackendData();
    IM_ASSERT(bd != nullptr && "No renderer backend to shutdown, or already shutdown?");
    ImGuiIO& io = ImGui::GetIO();
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

    ImGuiDestroyDeviceObjects();

    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
    platform_io.ClearRendererHandlers();
    IM_DELETE(bd);
}

void Loops::ImguiSystem::NewFrame(uint32_t frameInFlight)
{
    //if (!m_initialized)
    //{
    //    return;
    //}

    //// Reset the flag at the start of each frame
    //m_frameAlreadyRendered = false;

    ImGui::NewFrame();

    
}

void Loops::ImguiSystem::Render(uint32_t frameInFlight, const VkSemaphore& timelineSem, uint64_t signalValue, uint64_t waitValue)
{
    ImGuiNewFrame();

    // Create gui
    for (auto& func : m_guiDrawPersistentList)
    {
        func();
    }

    for (auto& func : m_guiDrawOnFramePersistentList)
    {
        func(frameInFlight);
    }

    // Rendering
    ImGui::Render();
    ImGuiData* bd = ImGuiGetBackendData();
    Loops::ImGuiInitInfo* v = &bd->m_vulkanInitInfo;
    ImDrawData* draw_data = ImGui::GetDrawData();
    const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
    if (!is_minimized)
    {
        {
            //Loops::VkUtils::ErrorCheck(vkResetCommandPool(v->m_device, m_commandPool, 0));
            VkCommandBufferBeginInfo info = {};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            info.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            Loops::VkUtils::ErrorCheck(vkBeginCommandBuffer(m_commandBuffers[frameInFlight], &info));
        }
        {
            vkCmdBeginRendering(m_commandBuffers[frameInFlight], &m_renderingInfoList[frameInFlight]);
        }

        // Record dear imgui primitives into command buffer
        ImGuiRenderDrawData(draw_data, m_commandBuffers[frameInFlight]);

        vkCmdEndRendering(m_commandBuffers[frameInFlight]);
        Loops::VkUtils::ErrorCheck(vkEndCommandBuffer(m_commandBuffers[frameInFlight]));

        // Submit command buffer
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
                Loops::VkUtils::ErrorCheck(vkQueueSubmit2(v->m_queue, 1, &submitInfo, VK_NULL_HANDLE));
            }
        }
    }
}

bool Loops::ImguiSystem::ImGuiCreateDeviceObjects()
{
    ImGuiData* bd = ImGuiGetBackendData();
    Loops::ImGuiInitInfo* v = &bd->m_vulkanInitInfo;
    VkResult err;

    if (!bd->m_descriptorSetLayoutTexture)
        ImGui_ImplVulkan_CreateDescriptorSetLayout(&bd->m_descriptorSetLayoutTexture, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
    if (!bd->m_descriptorSetLayoutSampler)
        ImGui_ImplVulkan_CreateDescriptorSetLayout(&bd->m_descriptorSetLayoutSampler, VK_DESCRIPTOR_TYPE_SAMPLER);

    if (v->m_descriptorPoolSize != 0)
    {
        IM_ASSERT(v->m_descriptorPoolSize >= IMGUI_IMPL_VULKAN_MINIMUM_SAMPLED_IMAGE_POOL_SIZE);
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, v->m_descriptorPoolSize },
            { VK_DESCRIPTOR_TYPE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE },
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = v->m_descriptorPoolSize + IMGUI_IMPL_VULKAN_MINIMUM_SAMPLER_POOL_SIZE;
        pool_info.poolSizeCount = (uint32_t)IM_COUNTOF(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        err = vkCreateDescriptorPool(v->m_device, &pool_info, v->m_allocator, &bd->m_descriptorPool);
        Loops::VkUtils::ErrorCheck(err);
    }

    // Create samplers
    // Bilinear sampling is required by default. Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines' or 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling.
    if (!bd->m_samplerLinear || !bd->m_samplerNearest)
    {
        VkSamplerCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter = VK_FILTER_LINEAR;
        info.minFilter = VK_FILTER_LINEAR;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.minLod = -1000;
        info.maxLod = 1000;
        info.maxAnisotropy = 1.0f;
        err = vkCreateSampler(v->m_device, &info, v->m_allocator, &bd->m_samplerLinear);
        Loops::VkUtils::ErrorCheck(err);
        bd->m_samplerLinearDS = ImGuiCreateSamplerDS(bd->m_samplerLinear);

        info.magFilter = VK_FILTER_NEAREST;
        info.minFilter = VK_FILTER_NEAREST;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        err = vkCreateSampler(v->m_device, &info, v->m_allocator, &bd->m_samplerNearest);
        Loops::VkUtils::ErrorCheck(err);
        bd->m_samplerNearestDS = ImGuiCreateSamplerDS(bd->m_samplerNearest);
    }

    // the below is happening in init
    //if (!bd->m_pipelineLayout)
    //{
    //    // Constants: we are using 'vec2 offset' and 'vec2 scale' instead of a full 3d projection matrix
    //    VkPushConstantRange push_constants[1] = {};
    //    push_constants[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    //    push_constants[0].offset = sizeof(float) * 0;
    //    push_constants[0].size = sizeof(float) * 4;
    //    VkDescriptorSetLayout set_layout[2] = { bd->m_descriptorSetLayoutTexture, bd->m_descriptorSetLayoutSampler };
    //    VkPipelineLayoutCreateInfo layout_info = {};
    //    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    //    layout_info.setLayoutCount = 2;
    //    layout_info.pSetLayouts = set_layout;
    //    layout_info.pushConstantRangeCount = 1;
    //    layout_info.pPushConstantRanges = push_constants;
    //    err = vkCreatePipelineLayout(v->m_device, &layout_info, v->m_allocator, &bd->m_pipelineLayout);
    //    Loops::VkUtils::ErrorCheck(err);
    //}

    // Create pipeline
//    bool create_main_pipeline = (v->m_pipelineInfoMain.RenderPass != VK_NULL_HANDLE);
//#ifdef IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
//    create_main_pipeline |= (v->m_useDynamicRendering && v->m_pipelineRenderingCreateInfo.sType == VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR);
//#endif
    //if (create_main_pipeline)
        //ImGuiCreateMainPipeline(&v->m_pipelineInfoMain);

    // Create command pool/buffer for texture upload
    if (!bd->m_texCommandPool)
    {
        VkCommandPoolCreateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        info.flags = 0;
        info.queueFamilyIndex = v->m_queueFamily;
        err = vkCreateCommandPool(v->m_device, &info, v->m_allocator, &bd->m_texCommandPool);
        Loops::VkUtils::ErrorCheck(err);
    }
    if (!bd->m_texCommandBuffer)
    {
        VkCommandBufferAllocateInfo info = {};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        info.commandPool = bd->m_texCommandPool;
        info.commandBufferCount = 1;
        err = vkAllocateCommandBuffers(v->m_device, &info, &bd->m_texCommandBuffer);
        Loops::VkUtils::ErrorCheck(err);
    }

    return true;
}

void Loops::ImguiSystem::ImGuiCreateMainPipeline(VkPipelineRenderingCreateInfoKHR* pipelineRenderingCreateInfo)
{
    ImGuiData* bd = ImGuiGetBackendData();
    Loops::ImGuiInitInfo* v = &bd->m_vulkanInitInfo;
    if (bd->m_pipeline)
    {
        vkDestroyPipeline(v->m_device, bd->m_pipeline, v->m_allocator);
        bd->m_pipeline = VK_NULL_HANDLE;
    }
    //ImGuiPipelineInfo* pipeline_info = &v->PipelineInfoMain;
    //if (pipeline_info != pipeline_info_in)
    //    *pipeline_info = *pipeline_info_in;

#ifdef IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
    //VkPipelineRenderingCreateInfoKHR* pipeline_rendering_create_info = &pipeline_info->m_pipelineRenderingCreateInfo;
    if (v->m_useDynamicRendering && pipelineRenderingCreateInfo->pColorAttachmentFormats != nullptr)
    {
        // Deep copy buffer to reduce error-rate for end user (#8282)
        ImVector<VkFormat> formats;
        formats.resize((int)pipelineRenderingCreateInfo->colorAttachmentCount);
        memcpy(formats.Data, pipelineRenderingCreateInfo->pColorAttachmentFormats, (size_t)formats.size_in_bytes());
        formats.swap(bd->m_pipelineRenderingCreateInfoColorAttachmentFormats);
        pipelineRenderingCreateInfo->pColorAttachmentFormats = bd->m_pipelineRenderingCreateInfoColorAttachmentFormats.Data;
    }
#endif
    bd->m_pipeline = ImGuiCreatePipeline(v->m_device, v->m_allocator, v->m_pipelineCache, *pipelineRenderingCreateInfo);
}

void Loops::ImguiSystem::ImGuiDestroyDeviceObjects()
{
    ImGuiData* bd = ImGuiGetBackendData();
    Loops::ImGuiInitInfo* v = &bd->m_vulkanInitInfo;
    ImGuiDestroyWindowRenderBuffers(v->m_device, &bd->m_mainWindowRenderBuffers, v->m_allocator);

    // Destroy all textures
    for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
        if (tex->RefCount == 1)
            ImGuiDestroyTexture(tex);

    if (bd->m_texCommandBuffer) { vkFreeCommandBuffers(v->m_device, bd->m_texCommandPool, 1, &bd->m_texCommandBuffer); bd->m_texCommandBuffer = VK_NULL_HANDLE; }
    if (bd->m_texCommandPool) { vkDestroyCommandPool(v->m_device, bd->m_texCommandPool, v->m_allocator); bd->m_texCommandPool = VK_NULL_HANDLE; }
    if (bd->m_samplerLinear) { vkDestroySampler(v->m_device, bd->m_samplerLinear, v->m_allocator); bd->m_samplerLinear = VK_NULL_HANDLE; }
    if (bd->m_samplerNearest) { vkDestroySampler(v->m_device, bd->m_samplerNearest, v->m_allocator); bd->m_samplerNearest = VK_NULL_HANDLE; }
    if (bd->m_shaderModuleVert) { vkDestroyShaderModule(v->m_device, bd->m_shaderModuleVert, v->m_allocator); bd->m_shaderModuleVert = VK_NULL_HANDLE; }
    if (bd->m_shaderModuleFrag) { vkDestroyShaderModule(v->m_device, bd->m_shaderModuleFrag, v->m_allocator); bd->m_shaderModuleFrag = VK_NULL_HANDLE; }
    if (bd->m_descriptorSetLayoutTexture) { vkDestroyDescriptorSetLayout(v->m_device, bd->m_descriptorSetLayoutTexture, v->m_allocator); bd->m_descriptorSetLayoutTexture = VK_NULL_HANDLE; }
    if (bd->m_descriptorSetLayoutSampler) { vkDestroyDescriptorSetLayout(v->m_device, bd->m_descriptorSetLayoutSampler, v->m_allocator); bd->m_descriptorSetLayoutSampler = VK_NULL_HANDLE; }
    if (bd->m_pipelineLayout) { vkDestroyPipelineLayout(v->m_device, bd->m_pipelineLayout, v->m_allocator); bd->m_pipelineLayout = VK_NULL_HANDLE; }
    if (bd->m_pipeline) { vkDestroyPipeline(v->m_device, bd->m_pipeline, v->m_allocator); bd->m_pipeline = VK_NULL_HANDLE; }
    if (bd->m_descriptorPool) { vkDestroyDescriptorPool(v->m_device, bd->m_descriptorPool, v->m_allocator); bd->m_descriptorPool = VK_NULL_HANDLE; }

    for (auto& cmdBuf : m_commandBuffers)
    {
        vkFreeCommandBuffers(v->m_device, m_commandPool, 1, &cmdBuf);
    }
    vkDestroyCommandPool(v->m_device, m_commandPool, v->m_allocator);
}


void Loops::ImguiSystem::UpdateBuffers(uint32_t frameInFlight)
{

}

void Loops::ImguiSystem::AddPersistentDrawCalls(const std::function<void()>& func)
{
    m_guiDrawPersistentList.push_back(func);
}

void Loops::ImguiSystem::AddPersistentDrawCalls(const std::function<void(uint32_t frameIndex)>& func)
{
    m_guiDrawOnFramePersistentList.push_back(func);
}


// callback ============

void Loops::ImguiSystem::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    //ImGuiIO& io = ImGui::GetIO(bd->Context);
    ImGuiIO& io = ImGui::GetIO();
    io.AddMouseWheelEvent((float)xoffset, (float)yoffset);
}

void Loops::ImguiSystem::KeyCallback(GLFWwindow* window, int keycode, int scancode, int action, int mods)
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

void Loops::ImguiSystem::WindowFocusCallback(GLFWwindow* window, int focused)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddFocusEvent(focused != 0);
}

void Loops::ImguiSystem::CursorPosCallback(GLFWwindow* window, double x, double y)
{
    //ImGuiIO& io = ImGui::GetIO(bd->Context);
    ImGuiIO& io = ImGui::GetIO();
    io.AddMousePosEvent((float)x, (float)y);
    m_lastValidMousePos = ImVec2((float)x, (float)y);
}

void Loops::ImguiSystem::CursorEnterCallback(GLFWwindow* window, int entered)
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

void Loops::ImguiSystem::CharCallback(GLFWwindow* window, unsigned int c)
{
    ImGuiIO& io = ImGui::GetIO();
    io.AddInputCharacter(c);
}

void Loops::ImguiSystem::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    ImGuiIO& io = ImGui::GetIO();

    UpdateKeyModifiers(io, window);
    if (button >= 0 && button < ImGuiMouseButton_COUNT)
        io.AddMouseButtonEvent(button, action == GLFW_PRESS);
}



#endif // #ifndef IMGUI_DISABLE