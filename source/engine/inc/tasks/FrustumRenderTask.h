#ifndef FRUSTUM_RENDER_TASK_H
#define FRUSTUM_RENDER_TASK_H

#include "Task.h"
#include "RenderData.h"
#include <glm/glm.hpp>

namespace Loops::Tasking
{
    class FrustumRenderTask : public GraphicsTask
    {
    private:
        struct View
        {
            glm::mat4 m_view;
            glm::mat4 m_projection;
            glm::mat4 m_viewProjectionActiveCamera;
        };

        std::vector<VkDescriptorSet> m_viewSet;
        VulkanBuffer m_cameraBuffer;

        void* m_cameraUniformMemoryPointer = nullptr;
        size_t m_cameraUniformDataSizePerFrame;

    public:
        FrustumRenderTask(const GraphicsTaskInfo& info, const VkPipelineRenderingCreateInfo& pipelineRenderingCreateInfo);
        void Update(VkCommandBuffer& commandBuffer, const uint32_t& frameInFlight, const glm::mat4& viewProjectionActiveCamera,
            const glm::mat4& viewMat, const glm::mat4& projectionMat);

        ~FrustumRenderTask();
    };
}

#endif // !FRUSTUM_RENDER_TASK_H
