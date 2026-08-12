#ifndef MATERIAL_MANAGER_H
#define MATERIAL_MANAGER_H

#include <flecs.h>
#include "Components.h"
#include "RenderData.h"
#include <vector>

namespace Loops
{
    constexpr uint32_t maxPbrMaterials = 20;
    class MaterialManager
    {
    private:
        //flecs::world& m_world;
        std::vector<PbrMaterial> m_pbrMaterials;
        uint32_t m_pbrMatCount = 0;

        std::unordered_map<uint32_t, Material> m_materials;
        uint32_t m_materialCount = 0;

    public:
        static constexpr uint32_t MAX_MATERIALS = 20;

        MaterialManager(/*flecs::world& world*/);
        ~MaterialManager();
        PbrMaterial* GetPbrMaterialRef();
        [[nodiscard]]
        uint32_t AddMaterial(const Material& material);
        const std::unordered_map<uint32_t, Loops::Material>& GetSceneMaterials() const;
    };
}
#endif // !MATERIAL_MANAGER_H
