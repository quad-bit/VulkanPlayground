#ifndef WIREFRAME_PIPELINE_H
#define WIREFRAME_PIPELINE_H

#include "Pipeline.h"
#include "Camera.h"
#include "SceneManager.h"
#include "VulkanManager.h"
#include "BoundsManager.h"
#include "imgui/ImguiSystem.h"
#include "tasks/WireFrameTask.h"
#include "tasks/BoundsRenderTask.h"
#include <memory>

namespace Loops::Tasking
{
    class WireframePipeline: public Pipeline
    {
    private:
        enum TimelineStages
        {
            UNINITIALIZED = 0,
            WIREFRAME_FINISHED = 1,
            BOUND_RENDER_FINISHED = 2,
            GUI_FINISHED = 3,
            SAFE_TO_PRESENT = 4,
            NUM_STAGES = 5
        };

        std::unique_ptr<WireFrameTask> m_pWireframeTask;
        std::unique_ptr<BoundsRenderTask> m_pBoundsRenderTask;
    protected:

    public:
        WireframePipeline(const PipelineInfo& info, const std::unique_ptr<VulkanManager>& pVulkanManager);
        virtual ~WireframePipeline();

        void Update(uint32_t currentFrameInFlight, const std::unique_ptr<SceneManager>& sceneManager, const BoundsManager& boundsManager,
            const std::unique_ptr<VulkanManager>& vulkanManager, const std::unique_ptr<ImguiSystem>& imguiUtil);
    };
}

#endif // !WIREFRAME_PIPELINE_H
