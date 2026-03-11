#pragma once
#include "Utils.h"
#include <array>

namespace Common
{
    //struct Transform
    //{
    //    glm::vec3 m_position{ 0.0f };
    //    glm::vec3 m_scale{ 1.0 };
    //    glm::vec3 m_eulerAngles{ 0.0 };
    //    glm::mat4x4 m_modelMat = glm::mat4(1.0);
    //    glm::mat4x4 m_modelMatGlobal = glm::mat4(1.0);

    //    Transform(){}

    //    Transform(glm::vec3 position, glm::vec3 scale, glm::vec3 eulerAngles) :
    //        m_position(position), m_scale(scale), m_eulerAngles(eulerAngles)
    //    {
    //        // Translate
    //        m_modelMat = glm::translate(m_modelMat, m_position);

    //        // 2. Rotate (order: X, Y, Z)
    //        m_modelMat = glm::rotate(m_modelMat, glm::radians(m_eulerAngles.x), glm::vec3(1.0f, 0.0f, 0.0f));
    //        m_modelMat = glm::rotate(m_modelMat, glm::radians(m_eulerAngles.y), glm::vec3(0.0f, 1.0f, 0.0f));
    //        m_modelMat = glm::rotate(m_modelMat, glm::radians(m_eulerAngles.z), glm::vec3(0.0f, 0.0f, 1.0f));

    //        // 3. Scale
    //        m_modelMat = glm::scale(m_modelMat, m_scale);
    //    }
    //};

    //class SceneManager;

    //class alignas(16) SceneNode
    //{
    //public:
    //    Transform m_transform = {};//16

    //private:
    //    mutable uint32_t m_index = 0;
    //    int m_parentIndex = -1; // stays -1 if scene root;

    //public:
    //    std::array<char, 16> m_name;

    //    bool m_hasChildren = false;
    //    bool m_hasMesh = false;
    //    //SceneNode(const SceneNode& parent) : m_parent(parent) {}

    //    const uint32_t& GetIndex() const;

    //    friend class SceneManager;
    //};
}