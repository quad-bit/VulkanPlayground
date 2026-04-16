#ifndef WIREFRAME_PIPELINE_H
#define WIREFRAME_PIPELINE_H

#include "Pipeline.h"
#include "Camera.h"
#include "SceneManager.h"
#include "VulkanManager.h"
#include "ImguiUtil.h"
#include "tasks/WireFrameTask.h"
#include <memory>

namespace Common
{
    class WireframePipeline: public Pipeline
    {
    private:
        enum TimelineStages
        {
            UNINITIALIZED = 0,
            WIREFRAME_FINISHED = 1,
            GUI_FINISHED = 2,
            SAFE_TO_PRESENT = 3,
            NUM_STAGES = 4
        };

        std::unique_ptr<WireFrameTask> m_pWireframeTask;
    protected:

    public:
        WireframePipeline(const PipelineInfo& info, std::shared_ptr<VulkanManager> pVulkanManager, std::unique_ptr<Common::Camera>& pCamera);
        virtual ~WireframePipeline();

        void Update(uint32_t currentFrameInFlight, std::shared_ptr<Common::SceneManager> sceneManager, std::shared_ptr < VulkanManager> vulkanManager,
            const std::shared_ptr<Common::ImguiUtil>& imguiUtil);
    };
}

#endif // !WIREFRAME_PIPELINE_H
