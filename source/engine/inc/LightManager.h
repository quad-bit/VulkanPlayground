#ifndef LIGHT_MANAGER_H
#define LIGHT_MANAGER_H

#include <mutex>
#include "Components.h"
#include "VulkanWrappers.h"
#include "RenderData.h"

namespace Loops
{
    class LightManager
    {
    private:
        static constexpr uint32_t s_maxCubeShadowMaps = 2;
        static LightManager* s_instancePtr;
        static std::mutex s_mtx;
        static constexpr uint16_t s_maxPointLights = 3;

        LightManager();
        void DeInitPrivate();

        std::vector<PointLight> m_pointLights{ s_maxPointLights };
        uint16_t m_pointLightCount = 0;
        DirectionalLight m_directionalLight;

        // descriptor set 0 will contain the light related stuff maybe binding 1 and 2
        // as descriptor set light data
        // shadow maps for max 3 lights (maybe closest to the camera)

        VkDescriptorSetLayoutBinding m_lightDataBinding, m_directionalShadowMapBinding, m_pointShadowMapBinding;
        std::vector<LightUniform> m_lightUniformArray{ MAX_LIGHTS };

    public:
        static constexpr uint32_t MAX_LIGHTS = 4;
        static LightManager* GetInstance();
        static void DeInit();
        /*void Init(const VkPhysicalDevice& physicalDevice,
            const VkDevice& device, const VkQueue& queue,
            uint32_t queuefamilyIndex, uint32_t maxFrameInFlights);*/
        void Init(flecs::world& world);
        PointLight* GetNewPointLight();
        DirectionalLight* GetDirectionalLight();

        const VkDescriptorSetLayoutBinding& GetLightDataBinding() const;
        const VkDescriptorSetLayoutBinding& GetPointShadowMapBinding() const;
        const VkDescriptorSetLayoutBinding& GetDirectionalShadowMapBinding() const;
        const std::vector<LightUniform>& GetLightUniformArray() const;
    };
}
#endif // !LIGHT_MANAGER_H
