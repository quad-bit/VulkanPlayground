#pragma once

#include <tiny_gltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "SceneManager.h"
#include "VulkanWrappers.h"
#include "Components.h"


namespace Common
{
    flecs::entity LoadGltf(const std::string_view& assetPath, flecs::world& world, Common::SceneManager& sceneManager,
        Common::VertexBuffer& vertexBuffer, Common::IndexBuffer& indexBuffer, float scaleFactor = 1.0f);
}