#ifndef GLTF_LOADER_H
#define GLTF_LOADER_H

#include <tiny_gltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "SceneManager.h"
#include "VulkanWrappers.h"
#include "Components.h"

namespace Common
{
    flecs::entity LoadGltf(const std::string_view& assetPath, flecs::world& world, Common::SceneManager& sceneManager,
        Common::VertexBuffer& vertexBuffer, Common::IndexBuffer& indexBuffer, uint32_t& numEntities, uint32_t& maxMeshViewsPerMesh, float scaleFactor = 1.0f);
}

#endif // !GLTF_LOADER_H
