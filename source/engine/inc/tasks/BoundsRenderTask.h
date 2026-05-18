#ifndef BOUNDS_RENDER_TASK_H
#define BOUNDS_RENDER_TASK_H

#include <vector>
#include "Defines.h"
#include "Task.h"
#include "VulkanWrappers.h"

namespace Loops::Tasking
{
    class BoundsRenderTask : public GraphicsTask
    {
    private:

        std::vector< glm::vec4 > m_cubeVerticies =
        {
            glm::vec4(-0.5f, -0.5f, -0.5f, 1.0f),
            glm::vec4(0.5f, -0.5f, -0.5f, 1.0f),
            glm::vec4(0.5f, 0.5f, -0.5f, 1.0f),
            glm::vec4(-0.5f, 0.5f, -0.5f, 1.0f),
            glm::vec4(-0.5f, -0.5f, 0.5f, 1.0f),
            glm::vec4(0.5f, -0.5f, 0.5f, 1.0f),
            glm::vec4(0.5f, 0.5f, 0.5f, 1.0f),
            glm::vec4(-0.5f, 0.5f, 0.5f, 1.0f)
        };

#if 0
        std::vector<uint32_t> m_cubeIndices =
        {
            0, 1, 3, 3, 1, 2,
            1, 5, 2, 2, 5, 6,
            5, 4, 6, 6, 4, 7,
            4, 0, 7, 7, 0, 3,
            3, 2, 7, 7, 2, 6,
            4, 5, 0, 0, 5, 1
        };
#else
        std::vector<uint32_t> m_cubeIndices =
        {
            0, 1, 1, 2, 2, 3, 3, 0,
            1, 5, 5, 6, 6, 2,
            6, 7, 7, 3,
            7, 4, 4, 5,
            4,0
        };
#endif

        const uint16_t TRANSFORM_SET = 0;
        struct BoundVertex
        {
            glm::vec4 m_pos;
        };

        Loops::VertexBuffer m_vertexBufferWrappers;
        Loops::IndexBuffer m_indexBufferWrappers;

        std::vector<VkDescriptorSet> m_transformSets;

        glm::mat4 m_transformArray[MAX_BOUNDS];
        Loops::VulkanBuffer m_transformBuffer;
        size_t m_transformUniformDataSizePerFrame;
        void* m_transformDataMemoryPointer = nullptr;

        void Init();

    public:
        BoundsRenderTask(const GraphicsTaskInfo& info);

        BoundsRenderTask(const GraphicsTaskInfo& info, const std::vector<VkImageView>& colorViews, const VkFormat& colorFormat);

        void Update(const uint32_t& frameInFlight, const VkSemaphore& timelineSem, uint64_t signalValue, std::optional<uint64_t> waitValue,
            const Loops::Bounds* boundArray, size_t numBounds, const glm::mat4& viewProjectionMat);

        void Update(const uint32_t& frameInFlight, const VkSemaphore& timelineSem, uint64_t signalValue, std::optional<uint64_t> waitValue,
            const Loops::Bounds* primitiveBoundArray, size_t numPrimitiveBounds, const Loops::Bounds* bvhNodeBoundArray, size_t numBvhNodeBounds,
            const glm::mat4& viewProjectionMat);


        ~BoundsRenderTask();
    };
}


#endif // !BOUNDS_RENDER_TASK_H
