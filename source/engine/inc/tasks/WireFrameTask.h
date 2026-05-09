#ifndef WIREFRAME_TASK_H
#define WIREFRAME_TASK_H

#include "Utils.h"
#include "Camera.h"
#include "RenderData.h"
#include "SceneManager.h"
#include "Task.h"
#include "VulkanWrappers.h"
#include <optional>
#include <memory>

namespace Loops::Tasking
{
    class WireFrameTask : public GraphicsTask
    {
    private:
        const uint16_t CAMERA_SET = 0;
        const uint16_t TRANSFORM_SET = 1;

        std::vector<VkDescriptorSet> m_transformSets;
        VulkanBuffer m_transformBuffer;

        std::vector<VkDescriptorSet> m_viewSet;
        VulkanBuffer m_cameraBuffer;
        const std::unique_ptr<Loops::Camera>& m_pCamera;

        void* m_transformUniformMemoryPointer = nullptr;
        void* m_cameraUniformMemoryPointer = nullptr;
        size_t m_transformUniformDataSizePerFrame;
        size_t m_cameraUniformDataSizePerFrame;

        void Init();

    public:
        WireFrameTask(const GraphicsTaskInfo& info, const std::unique_ptr<Loops::Camera>& pCamera);

        WireFrameTask(const GraphicsTaskInfo& info, const std::vector<VkImageView>& colorViews, const VkFormat& colorFormat,
            const std::unique_ptr<Loops::Camera>& pCamera);

        void Update(const uint32_t& frameInFlight, const VkSemaphore& timelineSem, uint64_t signalValue, std::optional<uint64_t> waitValue,
            const Loops::RenderData& renderData, const Loops::SceneManager& sceneManager);

        ~WireFrameTask();
    };
}

#endif // !WIREFRAME_TASK_H
