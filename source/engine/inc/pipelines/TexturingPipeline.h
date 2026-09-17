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
#include "effects/TranslucentEffect.h"
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
            TRANSLUCENT_COPY_FINISHED = 3,
            TRANSLUCENT_BACK_DEPTH_FINISHED = 4,
            TRANSLUCENT_FINISHED = 5,
            GUI_FINISHED = 6,
            SAFE_TO_PRESENT = 7,
            NUM_STAGES = 8
        };

        //std::unique_ptr<TextureUnlitTask> mp_textureUnlitTask;
        std::unique_ptr<PhongShadingTask> mp_phongShadingTask;
        std::unique_ptr<TranslucentEffect> mp_translucentEffect;
        const MaterialManager* m_materialManager = nullptr;
        std::vector<VkImage> m_defaultColorTargets, m_defaultDepthTargets;

    protected:

    public:
        TexturingPipeline(const VkUtils::VulkanContext * const vulkanContext,
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
