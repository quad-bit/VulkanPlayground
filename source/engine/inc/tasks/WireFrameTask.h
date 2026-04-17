#ifndef WIREFRAME_TASK_H
#define WIREFRAME_TASK_H

#include "Utils.h"
#include "Camera.h"
#include "RenderData.h"
#include "SceneManager.h"
#include "Task.h"

#include <optional>
#include <memory>

namespace Common::Tasking
{
    class WireFrameTask : public GraphicsTask
    {
    private:
        const uint16_t CAMERA_SET = 0;
        const uint16_t TRANSFORM_SET = 1;

        std::vector<VkDescriptorSet> m_transformSets;
        VkBuffer m_transformBuffer = VK_NULL_HANDLE;
        VkDeviceMemory m_transformUniformMemory = VK_NULL_HANDLE;

        std::vector<VkDescriptorSet> m_viewSet;
        VkBuffer m_cameraUniforms = VK_NULL_HANDLE;
        VkDeviceMemory m_cameraUniformMemory = VK_NULL_HANDLE;
        const std::unique_ptr<Common::Camera>& m_pCamera;

        void Init();

    public:
        WireFrameTask(const GraphicsTaskInfo& info, const std::unique_ptr<Common::Camera>& pCamera);

        WireFrameTask(const GraphicsTaskInfo& info, const std::vector<VkImageView>& colorViews, const std::vector<VkImageView>& depthViews,
            const VkFormat& colorFormat, const VkFormat& depthFormat, const std::unique_ptr<Common::Camera>& pCamera);

        void Update(const uint32_t& frameInFlight, const VkSemaphore& timelineSem, uint64_t signalValue, std::optional<uint64_t> waitValue,
            const Common::RenderData& renderData, const Common::SceneManager& sceneManager);

        ~WireFrameTask();
    };
}

#endif // !WIREFRAME_TASK_H
