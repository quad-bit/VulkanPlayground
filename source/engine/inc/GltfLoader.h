#pragma once

#include <tiny_gltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "SceneNode.h"
#include "SceneManager.h"

namespace Common
{
    /*class MeshData
    {
        std::vector<glm::vec3> m_positions;
        std::vector<glm::vec2> m_uv;
        std::vector<glm::vec3> m_normals;
        std::vector<glm::vec3> m_tangents;

        std::string m_name;

        std::vector<MeshData> m_children;
    };*/

    void LoadGltf(const std::string_view& assetPath, Common::SceneManager& sceneManager,
        std::vector<Common::Vertex>& vertexBuffer, std::vector<uint32_t>& indexBuffer);
}