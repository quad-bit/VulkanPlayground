#ifndef COLOR_UNLIT_H
#define COLOR_UNLIT_H

#include "Task.h"
#include "SceneManager.h"

namespace Common
{
    class ColorUnlitTask : public GraphicsTask
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
        std::shared_ptr<Common::Camera> m_pCamera;

        void Init();

    public:
        ColorUnlitTask(const char* name, const GraphicsTaskInfo& info, std::shared_ptr<Common::Camera> pCamera);

        ColorUnlitTask(const char* name, const GraphicsTaskInfo& info, const std::vector<VkImageView>& colorViews, const std::vector<VkImageView>& depthViews,
            const VkFormat& colorFormat, const VkFormat& depthFormat, std::shared_ptr<Common::Camera> pCamera);

        //Create quad draw specific resources
        void Update(const uint32_t& frameInFlight, const VkSemaphore& timelineSem, uint64_t signalValue, std::optional<uint64_t> waitValue,
            const Common::RenderData& renderData, const Common::SceneManager& sceneManager);

        virtual ~ColorUnlitTask();
    };
}

#endif // !COLOR_UNLIT_H
