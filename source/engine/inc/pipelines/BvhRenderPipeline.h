#ifndef BVH_RENDER_PIPELINE_H
#define BVH_RENDER_PIPELINE_H

#include "Pipeline.h"
#include "Camera.h"
#include "SceneManager.h"
#include "VulkanManager.h"
#include "BoundsManager.h"
#include "ImguiUtil.h"
#include "imgui/ImguiSystem.h"
#include "tasks/ColorUnlitTask.h"
#include "tasks/BoundsRenderTask.h"
#include <memory>

namespace Loops::Tasking
{
    class BvhRenderPipeline : public Pipeline
    {
    private:
        enum TimelineStages
        {
            UNINITIALIZED = 0,
            OPAQUE_FINISHED = 1,
            BOUND_RENDER_FINISHED = 2,
            GUI_FINISHED = 3,
            SAFE_TO_PRESENT = 4,
            NUM_STAGES = 5
        };

        std::unique_ptr<ColorUnlitTask> m_colorUnlitTaskPtr;
        std::unique_ptr<BoundsRenderTask> m_pBoundsRenderTask;

#ifdef BVH_SCENE_VIEW_ENABLED

#endif // BVH_SCENE_VIEW_ENABLED


    protected:

    public:
        BvhRenderPipeline(const PipelineInfo& info, const std::unique_ptr<VulkanManager>& pVulkanManager, const std::unique_ptr<ImguiSystem>& imguiUtil);
        virtual ~BvhRenderPipeline();

        void Update(uint32_t currentFrameInFlight, const std::unique_ptr<SceneManager>& sceneManager, const BoundsManager& boundsManager,
            const std::unique_ptr<VulkanManager>& vulkanManager, const std::unique_ptr<ImguiSystem>& imguiUtil);
    };
}

#endif // !BVH_RENDER_PIPELINE_H
