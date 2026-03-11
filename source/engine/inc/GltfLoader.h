#pragma once

#include <tiny_gltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "SceneManager.h"
#include "Components.h"


namespace Common
{
    void LoadGltf(const std::string_view& assetPath, Common::SceneManager& sceneManager,
        std::vector<Common::Vertex>& vertexBuffer, std::vector<uint32_t>& indexBuffer);
}