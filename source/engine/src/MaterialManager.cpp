#include "MaterialManager.h"
#include "Assertion.h"

Loops::MaterialManager::MaterialManager(/*flecs::world& world*/)
{
    m_pbrMaterials.resize(maxPbrMaterials);
}

Loops::MaterialManager::~MaterialManager()
{
}

Loops::PbrMaterial* Loops::MaterialManager::GetPbrMaterialRef()
{
    auto pbr = &m_pbrMaterials[m_pbrMatCount];
    ASSERT_MSG_DEBUG(m_pbrMatCount++ < maxPbrMaterials, "out of range");
    return pbr;
}

uint32_t Loops::MaterialManager::AddMaterial(const Loops::Material& material)
{
    const auto materialIndex = m_materialCount++;
    m_materials.insert({ materialIndex, material });
    return materialIndex;
}

const std::unordered_map<uint32_t, Loops::Material>& Loops::MaterialManager::GetSceneMaterials() const
{
    return m_materials;
}
