#ifndef MATERIAL_MANAGER_H
#define MATERIAL_MANAGER_H

#include <flecs.h>
#include "Components.h"

namespace Loops
{
    constexpr uint32_t maxPbrMaterials = 20;
    class MaterialManager
    {
    private:
        //flecs::world& m_world;
        std::vector<PbrMaterial> m_pbrMaterials;
        uint32_t m_pbrMatCount = 0;

    public:
        MaterialManager(/*flecs::world& world*/);
        ~MaterialManager();
        PbrMaterial* GetPbrMaterialRef();
    };
}
#endif // !MATERIAL_MANAGER_H
