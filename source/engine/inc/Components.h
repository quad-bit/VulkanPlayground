#ifndef COMPONENTS_H
#define COMPONENTS_H

#include "Utils.h"
#include "Defines.h"

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

namespace Loops
{
    // ============= TRANSFORM ===============
    struct Transform
    {
        glm::vec3 m_position{ 0.0f };
        glm::vec3 m_scale{ 1.0 };
        glm::vec3 m_eulerAngles{ 0.0 };
        glm::mat4x4 m_modelMat = glm::mat4(1.0);
        glm::mat4x4 m_modelMatGlobal = glm::mat4(1.0);
    };
    // ============= TRANSFORM ===============

    // ============= MESH ===============

    // one or more per object with a mesh (more in case of submesh)
    struct alignas(4) MeshView
    {
        uint32_t m_firstIndex, m_indexCount;
        uint32_t m_viewIndex;
    };

    // Multiple submeshes or primitives in one
    struct Mesh
    {
        MeshView m_meshViews[MAX_MESH_VIEWS_PER_MESH];
        uint16_t m_meshViewCount = 0;
        // ids of Vulkan wrappers
        uint16_t m_vertexBufferIndex, m_indexBufferIndex;
    };
    // ============= MESH ===============

    // ============= MATERIAL ===============
    enum class EFFECT_TYPE // passes containing tasks
    {
        OPAQUE_EFT,
        APLHA_MASK_EFT,
        TRANSPARENT_EFT
    };

    enum class TECHNIQUE_TYPE // individual tasks
    {
        PBR,
        PBR_DOUBLE_SIDED,
        UNLIT,
        UNLIT_DOUBLE_SIDED,
        LAMBERTIAN,
        LAMBERTIAN_DOUBLE_SIDED,
    };

    struct MaterialData;

    struct Material
    {
        EFFECT_TYPE m_effect;
        TECHNIQUE_TYPE m_techniqueType;
        MaterialData* m_materialData;
    };

    class MaterialData
    {
    public:
        glm::vec4 m_baseColorFactor = glm::vec4(1.0f);
        int m_baseColorTextureIndex;
        uint32_t m_baseTextureCoordinateSet; // uv0 vs uv1
    };

    class PbrMaterial : public MaterialData
    {
    public:
        glm::vec4 m_emissiveFactor = glm::vec4(0.0f);

        int m_metallicRoughnessTextureIndex = -1;
        int m_normalTextureIndex = -1;
        int m_occlusionTextureIndex = -1;
        int m_emissiveTextureIndex = -1;

        float m_alphaCutoff = 1.0f;
        float m_metallicFactor = 1.0f;
        float m_roughnessFactor = 1.0f;
        float m_emissiveStrength = 1.0f;

        struct TexCoordSets
        {
            uint8_t m_metallicRoughnessCoords = 0;
            uint8_t m_specularGlossinessCoords = 0;
            uint8_t m_normalCoords = 0;
            uint8_t m_occlusionCoords = 0;
            uint8_t m_emissiveCoords = 0;
        } m_texCoordSets;

        struct Extension
        {
            int m_specularGlossinessTextureIndex = -1;
            int m_diffuseTextureIndex = -1;
            glm::vec4 m_diffuseFactor = glm::vec4(1.0f);
            glm::vec3 m_specularFactor = glm::vec3(0.0f);
        }m_extension;

        enum PbrWorkflows
        {
            METALLIC_ROUGHNESS,
            SPECULAR_GLOSSINESS,
            NONE
        }m_pbrWorkflows{ NONE };
    };
    // ============= MATERIAL ===============

}

#endif