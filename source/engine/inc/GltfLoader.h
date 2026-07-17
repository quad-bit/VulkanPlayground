#ifndef GLTF_LOADER_H
#define GLTF_LOADER_H

#include <tiny_gltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "SceneManager.h"
#include "BoundsManager.h"
#include "VulkanWrappers.h"
#include "Components.h"
#include "MaterialManager.h"

namespace Loops
{
    flecs::entity LoadGltf(const std::string_view& assetPath,
        flecs::world& world, Loops::SceneManager& sceneManager,
        Loops::BoundsManager& boundsManager, Loops::VertexBuffer& vertexBuffer,
        Loops::IndexBuffer& indexBuffer, uint32_t& numEntities,
        uint32_t& maxMeshViewsPerMesh, Loops::MaterialManager* pMaterialManager,
        float scaleFactor = 1.0f);
}

#endif // !GLTF_LOADER_H
