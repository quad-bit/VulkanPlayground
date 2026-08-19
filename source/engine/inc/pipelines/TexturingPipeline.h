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
//#include "tasks/BoundsRenderTask.h"
#include "tasks/PhongShadingTask.h"
#include <memory>

namespace Loops::Tasking
{
    class TexturingPipeline : public Pipeline
    {
    private:
        enum TimelineStages
        {
            UNINITIALIZED = 0,
            SHADOW_PASS_FINISHED = 1,
            OPAQUE_FINISHED = 2,
            GUI_FINISHED = 3,
            SAFE_TO_PRESENT = 4,
            NUM_STAGES = 5
        };

        //std::unique_ptr<TextureUnlitTask> mp_textureUnlitTask;
        std::unique_ptr<PhongShadingTask> mp_phongShadingTask;
        const MaterialManager* m_materialManager = nullptr;

    protected:

    public:
        TexturingPipeline(const PipelineInfo& info,
            const std::unique_ptr<VulkanManager>& pVulkanManager,
            const std::unique_ptr<ImguiSystem>& imguiUtil,
            const Loops::MaterialManager* materialManager,
            const std::unique_ptr<SceneManager>& sceneManager);

        virtual ~TexturingPipeline();

        void Update(uint32_t currentFrameInFlight,
            const std::unique_ptr<SceneManager>& sceneManager,
            const BoundsManager& boundsManager,
            const std::unique_ptr<VulkanManager>& vulkanManager,
            const std::unique_ptr<ImguiSystem>& imguiUtil);
    };
}

#endif // TEXTURING_PIPELINE_H
