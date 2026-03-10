#include "ValidationManager.h"
#include <assert.h>

namespace
{
    // ================= new debug utils starts

    static const char* DebugAnnotObjectToString(const VkObjectType object_type)
    {
        switch (object_type)
        {
        case VK_OBJECT_TYPE_INSTANCE:
            return "VkInstance";
        case VK_OBJECT_TYPE_PHYSICAL_DEVICE:
            return "VkPhysicalDevice";
        case VK_OBJECT_TYPE_DEVICE:
            return "VkDevice";
        case VK_OBJECT_TYPE_QUEUE:
            return "VkQueue";
        case VK_OBJECT_TYPE_SEMAPHORE:
            return "VkSemaphore";
        case VK_OBJECT_TYPE_COMMAND_BUFFER:
            return "VkCommandBuffer";
        case VK_OBJECT_TYPE_FENCE:
            return "VkFence";
        case VK_OBJECT_TYPE_DEVICE_MEMORY:
            return "VkDeviceMemory";
        case VK_OBJECT_TYPE_BUFFER:
            return "VkBuffer";
        case VK_OBJECT_TYPE_IMAGE:
            return "VkImage";
        case VK_OBJECT_TYPE_EVENT:
            return "VkEvent";
        case VK_OBJECT_TYPE_QUERY_POOL:
            return "VkQueryPool";
        case VK_OBJECT_TYPE_BUFFER_VIEW:
            return "VkBufferView";
        case VK_OBJECT_TYPE_IMAGE_VIEW:
            return "VkImageView";
        case VK_OBJECT_TYPE_SHADER_MODULE:
            return "VkShaderModule";
        case VK_OBJECT_TYPE_PIPELINE_CACHE:
            return "VkPipelineCache";
        case VK_OBJECT_TYPE_PIPELINE_LAYOUT:
            return "VkPipelineLayout";
        case VK_OBJECT_TYPE_RENDER_PASS:
            return "VkRenderPass";
        case VK_OBJECT_TYPE_PIPELINE:
            return "VkPipeline";
        case VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT:
            return "VkDescriptorSetLayout";
        case VK_OBJECT_TYPE_SAMPLER:
            return "VkSampler";
        case VK_OBJECT_TYPE_DESCRIPTOR_POOL:
            return "VkDescriptorPool";
        case VK_OBJECT_TYPE_DESCRIPTOR_SET:
            return "VkDescriptorSet";
        case VK_OBJECT_TYPE_FRAMEBUFFER:
            return "VkFramebuffer";
        case VK_OBJECT_TYPE_COMMAND_POOL:
            return "VkCommandPool";
        case VK_OBJECT_TYPE_SURFACE_KHR:
            return "VkSurfaceKHR";
        case VK_OBJECT_TYPE_SWAPCHAIN_KHR:
            return "VkSwapchainKHR";
        case VK_OBJECT_TYPE_DEBUG_REPORT_CALLBACK_EXT:
            return "VkDebugReportCallbackEXT";
        case VK_OBJECT_TYPE_DISPLAY_KHR:
            return "VkDisplayKHR";
        case VK_OBJECT_TYPE_DISPLAY_MODE_KHR:
            return "VkDisplayModeKHR";
            /*case VK_OBJECT_TYPE_OBJECT_TABLE_NVX:
                return "VkObjectTableNVX";
            case VK_OBJECT_TYPE_INDIRECT_COMMANDS_LAYOUT_NVX:
                return "VkIndirectCommandsLayoutNVX";*/
        case VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_KHR:
            return "VkDescriptorUpdateTemplateKHR";
        default:
            return "Unknown Type";
        }
    }

    // Define a callback to capture the messages
    VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData) {


        char prefix[64];
        char* message = (char*)malloc(strlen(callbackData->pMessage) + 1000);
        
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
            strcpy(prefix, "VERBOSE : ");
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
            strcpy(prefix, "INFO : ");
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            strcpy(prefix, "WARNING : ");
        }
        else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            strcpy(prefix, "ERROR : ");
        }
        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
            strcat(prefix, "GENERAL");
        }
        else {
            /*if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_SPECIFICATION_BIT_EXT) {
                strcat(prefix, "SPEC");
                validation_error = 1;
            }*/
            if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
                /*if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_SPECIFICATION_BIT_EXT) {
                    strcat(prefix, "|");
                }*/
                strcat(prefix, "PERF");
            }
        }

        sprintf(message,
            "%s - Message ID Number %d, Message ID String :\n%s, Message : \n%s",
            prefix,
            callbackData->messageIdNumber,
            callbackData->pMessageIdName,
            callbackData->pMessage);
        if (callbackData->objectCount > 0) {
            char tmp_message[500];
            sprintf(tmp_message, "\n Objects - %d\n", callbackData->objectCount);
            strcat(message, tmp_message);
            for (uint32_t object = 0; object < callbackData->objectCount; ++object) {
                sprintf(tmp_message,
                    " Object[%d] - Type %s, Value %p, Name \"%s\"\n",
                    object,
                    DebugAnnotObjectToString(
                        callbackData->pObjects[object].objectType),
                    (void*)(callbackData->pObjects[object].objectHandle),
                    callbackData->pObjects[object].pObjectName);
                strcat(message, tmp_message);
            }
        }
        if (callbackData->cmdBufLabelCount > 0) {
            char tmp_message[500];
            sprintf(tmp_message,
                "\n Command Buffer Labels - %d\n",
                callbackData->cmdBufLabelCount);
            strcat(message, tmp_message);
            for (uint32_t label = 0; label < callbackData->cmdBufLabelCount; ++label) {
                sprintf(tmp_message,
                    " Label[%d] - %s { %f, %f, %f, %f}\n",
                    label,
                    callbackData->pCmdBufLabels[label].pLabelName,
                    callbackData->pCmdBufLabels[label].color[0],
                    callbackData->pCmdBufLabels[label].color[1],
                    callbackData->pCmdBufLabels[label].color[2],
                    callbackData->pCmdBufLabels[label].color[3]);
                strcat(message, tmp_message);
            }
        }
        printf("%s\n", message);
        fflush(stdout);
        free(message);
        // Don't bail out, but keep going.
        return false;
    }

    // ================= new debug utils Ends
}

void ValidationManager::AddRequiredPlatformInstanceExtensions(std::vector<const char*>* instance_extensions)
{
#if defined(GLFW_ENABLED)
    uint32_t instance_extension_count = 0;
    const char ** instance_extensions_buffer = glfwGetRequiredInstanceExtensions(&instance_extension_count);
    for (uint32_t i = 0; i < instance_extension_count; i++) 
    {
        instance_extensions->push_back(instance_extensions_buffer[i]);
    }

#elif VK_USE_PLATFORM_WIN32_KHR
    instance_extensions->push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif VK_USE_PLATFORM_XCB_KHR
    instance_extensions->push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif
}

ValidationManager::ValidationManager()
{
    SetupLayersAndExtensions();
    SetupDebug();
}

ValidationManager::~ValidationManager()
{
    vulkanInstanceRef = nullptr;
    pAllocatorRef = nullptr;
}

//================================================  DEBUGGING

void ValidationManager::SetupDebug()
{
#if _DEBUG

    instanceLayerNameList.push_back("VK_LAYER_KHRONOS_validation");
    instanceExtensionNameList.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    dbg_messenger_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    dbg_messenger_create_info.pNext = NULL;
    dbg_messenger_create_info.flags = 0;
    dbg_messenger_create_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    dbg_messenger_create_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    dbg_messenger_create_info.pfnUserCallback = debug_messenger_callback;
    dbg_messenger_create_info.pUserData = NULL;
#endif
}

PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXTCall = nullptr;
PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXTCall = nullptr;
PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXTCall = nullptr;
PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXTCall = nullptr;

void ValidationManager::InitDebug(VkInstance * vulkanInstance, VkAllocationCallbacks* pAllocator)
{
#if _DEBUG
    vulkanInstanceRef = vulkanInstance;
    pAllocatorRef = pAllocator;
    CreateDebugUtilsMessengerEXT =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            *vulkanInstanceRef,
            "vkCreateDebugUtilsMessengerEXT");
    DestroyDebugUtilsMessengerEXT =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            *vulkanInstanceRef,
            "vkDestroyDebugUtilsMessengerEXT");

    VkResult result = CreateDebugUtilsMessengerEXT(*vulkanInstanceRef,
        &dbg_messenger_create_info,
        NULL,
        &dbg_messenger);

    vkSetDebugUtilsObjectNameEXTCall = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(*vulkanInstanceRef, "vkSetDebugUtilsObjectNameEXT");
    vkCmdBeginDebugUtilsLabelEXTCall = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(*vulkanInstanceRef, "vkCmdBeginDebugUtilsLabelEXT");
    vkCmdEndDebugUtilsLabelEXTCall = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(*vulkanInstanceRef, "vkCmdEndDebugUtilsLabelEXT");
    vkCmdInsertDebugUtilsLabelEXTCall = (PFN_vkCmdInsertDebugUtilsLabelEXT)vkGetInstanceProcAddr(*vulkanInstanceRef, "vkCmdInsertDebugUtilsLabelEXT");

    if (vkSetDebugUtilsObjectNameEXTCall == nullptr ||
        vkCmdBeginDebugUtilsLabelEXTCall == nullptr ||
        vkCmdEndDebugUtilsLabelEXTCall == nullptr ||
        vkCmdInsertDebugUtilsLabelEXTCall == nullptr)
    {
        assert(0);
        std::exit(-1);
    }

    // new debug utils
#endif
}

void ValidationManager::DeinitDebug()
{
#if _DEBUG    //vkDestroyDebugReportCallbackEXTObj(*vulkanInstanceRef, debugReport, pAllocatorRef);
    DestroyDebugUtilsMessengerEXT(*vulkanInstanceRef, dbg_messenger, NULL);
#endif
}

void ValidationManager::SetupLayersAndExtensions()
{
    instanceExtensionNameList.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    instanceExtensionNameList.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    deviceExtensionNameList.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    deviceExtensionNameList.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    deviceExtensionNameList.push_back(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);

    AddRequiredPlatformInstanceExtensions(&instanceExtensionNameList);
}

//
//void DebugMarker::SetObjectName(VkObjectType objectType, uint64_t objectHandle, const char* objectName)
//{
//    VkDebugUtilsObjectNameInfoEXT name_info = { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
//    name_info.objectType = objectType;
//    name_info.objectHandle = objectHandle;
//    name_info.pObjectName = objectName;
//    vkSetDebugUtilsObjectNameEXTCall(VulkanDeviceInfo::GetLogicalDevice(), &name_info);
//}
//
//void SetCommandBufferBeginLabel(VkCommandBuffer commandBuffer, const char* labelName, std::vector<float> color)
//{
//    VkDebugUtilsLabelEXT label = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
//    label.pLabelName = labelName;
//    label.color[0] = color[0];
//    label.color[1] = color[1];
//    label.color[2] = color[2];
//    label.color[3] = color[3];
//    vkCmdBeginDebugUtilsLabelEXTCall(commandBuffer, &label);
//}
//
//void DebugMarker::InsertLabelInCommandBuffer(VkCommandBuffer commandBuffer, const char* labelName, std::vector<float> color)
//{
//    VkDebugUtilsLabelEXT label = { VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
//    label.pLabelName = labelName;
//    label.color[0] = color[0];
//    label.color[1] = color[1];
//    label.color[2] = color[2];
//    label.color[3] = color[3];
//    vkCmdInsertDebugUtilsLabelEXTCall(commandBuffer, &label);
//}
//
//void DebugMarker::SetCommandBufferEndLabel(VkCommandBuffer commandBuffer)
//{
//    vkCmdEndDebugUtilsLabelEXTCall(commandBuffer);
//}
