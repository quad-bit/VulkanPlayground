#ifndef TEXTURING_PIPELINE_H
#define TEXTURING_PIPELINE_H

#include "Pipeline.h"
#include "Camera.h"
#include "SceneManager.h"
#include "VulkanManager.h"
#include "BoundsManager.h"
#include "ImguiUtil.h"
#include "imgui/ImguiSystem.h"
#include "tasks/TextureUnlitTask.h"
#include "tasks/BoundsRenderTask.h"
#include <memory>

namespace Loops::Tasking
{
    class TexturingPipeline : public Pipeline
    {
    private:
        enum TimelineStages
        {
            UNINITIALIZED = 0,
            OPAQUE_FINISHED = 1,
            GUI_FINISHED = 2,
            SAFE_TO_PRESENT = 3,
            NUM_STAGES = 4
        };

        std::unique_ptr<TextureUnlitTask> mp_textureUnlitTask;
        const MaterialManager* m_materialManager = nullptr;

    protected:

    public:
        TexturingPipeline(const PipelineInfo& info,
            const std::unique_ptr<VulkanManager>& pVulkanManager,
            const std::unique_ptr<ImguiSystem>& imguiUtil,
            const Loops::MaterialManager* materialManager);

        virtual ~TexturingPipeline();

        void Update(uint32_t currentFrameInFlight,
            const std::unique_ptr<SceneManager>& sceneManager,
            const BoundsManager& boundsManager,
            const std::unique_ptr<VulkanManager>& vulkanManager,
            const std::unique_ptr<ImguiSystem>& imguiUtil);
    };
}

#endif // TEXTURING_PIPELINE_H
