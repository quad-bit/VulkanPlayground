#include "LightManager.h"
#include "Assertion.h"

// Initialize static members
Loops::LightManager* Loops::LightManager::s_instancePtr = nullptr;
std::mutex Loops::LightManager::s_mtx;

Loops::LightManager* Loops::LightManager::GetInstance()
{
    if (s_instancePtr == nullptr)
    {
        std::lock_guard<std::mutex> lock(s_mtx);
        if (s_instancePtr == nullptr)
        {
            s_instancePtr = new Loops::LightManager();
        }
    }
    return s_instancePtr;
}


Loops::LightManager::LightManager()
{
    m_lightDataBinding = 
    {
        1, // binding slot
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        1, // num bindings
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        nullptr // mutable sampler
    };

    m_directionalShadowMapBinding =
    {
        2,//binding location
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        1,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        nullptr
    };

    m_pointShadowMapBinding = 
    {
        3,//binding location
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        s_maxCubeShadowMaps,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        nullptr
    };
}

void Loops::LightManager::DeInitPrivate()
{
}

void Loops::LightManager::DeInit()
{
}

//void Loops::LightManager::Init(const VkPhysicalDevice& physicalDevice, const VkDevice& device, const VkQueue& queue, uint32_t queuefamilyIndex, uint32_t maxFrameInFlights)
//{
//}

void Loops::LightManager::Init(flecs::world& world)
{
    world.system<Transform, Light>("LightSystem")
        //.kind(0)
        .each(
            [this](Transform& t, Light& l)
            {
                const LIGHT_TYPE lightType = l.m_type;
                if (lightType == LIGHT_TYPE::DIRECTIONAL)
                {
                    auto translationMat = glm::translate(t.m_position);
                    auto scaleMat = glm::scale(t.m_scale);

                    glm::mat4 rotXMat = glm::rotate(t.m_eulerAngles.x, glm::vec3(1, 0, 0));
                    glm::mat4 rotYMat = glm::rotate(t.m_eulerAngles.y, glm::vec3(0, 1, 0));
                    glm::mat4 rotZMat = glm::rotate(t.m_eulerAngles.z, glm::vec3(0, 0, 1));

                    auto rotationMat = rotZMat * rotYMat * rotXMat;

                    t.m_modelMat = translationMat * rotationMat * scaleMat;

                    glm::vec3 forward{ t.m_modelMat[2] };
                    forward = glm::normalize(forward);

                    DirectionalLight* dirLight = static_cast<DirectionalLight*>(l.m_data);
                    dirLight->m_direction = forward;

                    // directional light is always at 0
                    m_lightUniformArray[0].m_ambient = glm::vec4(dirLight->m_ambient.x, dirLight->m_ambient.y, dirLight->m_ambient.z, 1.0);
                    m_lightUniformArray[0].m_diffuse = glm::vec4(dirLight->m_diffuse.x, dirLight->m_diffuse.y, dirLight->m_diffuse.z, 1.0);
                    m_lightUniformArray[0].m_specular = glm::vec4(dirLight->m_specular.x, dirLight->m_specular.y, dirLight->m_specular.z, 1.0);;
                    m_lightUniformArray[0].m_lightMatrix = t.m_modelMat;
                    m_lightUniformArray[0].m_posOrDir = glm::vec4{ forward.x, forward.y, forward.z, 0.0 };
                    m_lightUniformArray[0].m_shadowMapIndex = -1;
                }
                else
                    ASSERT_MSG_DEBUG(0, "not yet handled");
            }
        );
}

Loops::PointLight* Loops::LightManager::GetNewPointLight()
{
    auto index = m_pointLightCount++;
    Loops::ASSERT_MSG_DEBUG(m_pointLightCount < s_maxPointLights, "Out of range");
    return &m_pointLights[index];
}

Loops::DirectionalLight* Loops::LightManager::GetDirectionalLight()
{
    return &m_directionalLight;
}

const VkDescriptorSetLayoutBinding& Loops::LightManager::GetLightDataBinding() const
{
    return m_lightDataBinding;
}

const VkDescriptorSetLayoutBinding& Loops::LightManager::GetPointShadowMapBinding() const
{
    return m_pointShadowMapBinding;
}

const VkDescriptorSetLayoutBinding& Loops::LightManager::GetDirectionalShadowMapBinding() const
{
    return m_directionalShadowMapBinding;
}

const std::vector<Loops::LightUniform>& Loops::LightManager::GetLightUniformArray() const
{
    return m_lightUniformArray;
}
