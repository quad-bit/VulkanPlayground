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
