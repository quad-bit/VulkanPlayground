#version 450 
//#extension GL_ARB_separate_shader_objects : enable 
// std430 + UBO 
#extension GL_EXT_scalar_block_layout : enable 
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) out vec4 outColor;
layout (constant_id = 0) const uint MAX_LIGHTS = 4;

struct InVertex
{
    //vec4 color;
    vec2 fragTexCoord;
    vec3 normal;
    vec4 tangent;
    vec4 fragPos;
    vec4 posInLightSpace[MAX_LIGHTS];
    mat3 TBN;
};
layout (location = 0) in InVertex inVertex;

layout (std430, set = 0, binding = 0) uniform Scene
{
    mat4 view;
    mat4 projection;
    vec4 cameraPos;
} scene;

// ========= LIGHT ===========>>
struct Light
{
    mat4 lightMatrix;
    vec4 posOrDir; // w=0 for directional, w=1 for point lights
    vec4 ambient;
    vec4 specular;
    vec4 diffuse;
    int shadowMapIndex; //
};
layout (std430, set = 0, binding = 1) uniform LightData
{
    Light lights[MAX_LIGHTS]; // at 0 directional light, rest point lights
};


layout (set = 0, binding = 2) uniform sampler2D directionalShadowMap;
//layout (set = 0, binding = 3) uniform samplerCube pointShadowMaps[];

// <<======== LIGHT ===========

layout (set = 2, binding = 0) uniform sampler2D textures[];

struct Material
{
    vec4 color;
    int diffuseMapIndex;
    int normalMapIndex;
};
layout(std430, set = 3, binding = 0) readonly buffer MaterialUniform
{
    Material materials[];
};

layout(push_constant) uniform PushConstsVert
{
    int transformIndex;
    int materialIndex;
    //int lightIndicies[3];
};

// ====================================
float ShadowCalculationDirectional(int lightIndex, vec3 inNormal)
{
    vec3 projectedCoords = vec3(inVertex.posInLightSpace[lightIndex].xyz/inVertex.posInLightSpace[lightIndex].w);
    
    float tempZ = projectedCoords.z;
    projectedCoords.y *= -1.0f;
    projectedCoords = projectedCoords * 0.5f + vec3(0.5f);
    projectedCoords.z = tempZ;

    if (projectedCoords.z > 1.0 || projectedCoords.z < 0.0) {
        return 1.0; 
    }

    float closestDepth = texture(directionalShadowMap, projectedCoords.xy).r;
    float currentDepth = projectedCoords.z;

    // Dynamic bias calculation to alleviate shadow acne
    float bias = max(0.005 * (1.0 - dot(normalize(inNormal), vec3(0.0, 1.0, 0.0))), 0.0005);
    float shadow  = currentDepth - bias > closestDepth ? 1.0f : 0.0f;
    return shadow;

    // float shadowFactor = 0.0;
    // vec2 texelSize = 1.0 / textureSize(directionalShadowMap, 0);
    // // 3x3 Kernel PCF
    // for(int x = -1; x <= 1; ++x)
    // {
    //     for(int y = -1; y <= 1; ++y)
    //     {
    //         vec3 sampleCoord = vec3(projectedCoords.xy + vec2(x, y) * texelSize, projectedCoords.z - bias);
    //         shadowFactor += texture(directionalShadowMap, sampleCoord.xy).r; 
    //     }
    // }
    // return shadowFactor / 9.0;
}

vec4 CalculateDirectionalLightColor(Light directionalLight, vec3 normal, vec3 viewDir, int diffuseIndex, int specularIndex)
{
    vec3 lightDir = normalize(-directionalLight.posOrDir.xyz);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    float shininess = 32.0f;// material.shininess
    vec3 reflectDir = reflect(lightDir, normal); //-lightDir
    // float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    // float strength = 0.5f;
    vec3 halfWayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfWayDir), 0.0), shininess);

    // combine results
    vec4 diffuseComponent = texture(textures[diffuseIndex], inVertex.fragTexCoord);
    vec4 specularComponent = specularIndex > -1 ? texture(textures[specularIndex], inVertex.fragTexCoord) : vec4(1.0f);
    vec4 ambient = directionalLight.ambient * diffuseComponent;
    vec4 diffuse = directionalLight.diffuse * diff * diffuseComponent;
    vec4 specular = directionalLight.specular * spec * specularComponent;

    float shadow = ShadowCalculationDirectional(0, inVertex.normal);

    return (ambient + (1.0 - shadow) * (diffuse + specular));
}

vec4 CalculatePointLightColor(Light pointLight)
{
    return vec4(1.0);
}

void main() 
{
    int diffuseMapIndex = materials[materialIndex].diffuseMapIndex;
    int normalMapIndex = materials[materialIndex].normalMapIndex;
    int specularMapIndex = -1;

    vec4 color = texture(textures[diffuseMapIndex], inVertex.fragTexCoord);

    vec3 V = normalize(scene.cameraPos.xyz - inVertex.fragPos.xyz);
    V = normalize(V);
    // vec3 N = GetNormal(vec4(inVertex.normal.xyz, 1.0f), inVertex.tangent, normalMapIndex, inVertex.fragTexCoord);
    vec3 normalFromTexture = texture(textures[normalMapIndex], inVertex.fragTexCoord).rgb;
    normalFromTexture = normalize(normalFromTexture * 2.0 - 1.0);
    vec3 N = inVertex.TBN * normalFromTexture;

    vec4 result = CalculateDirectionalLightColor(lights[0], N, V, diffuseMapIndex, specularMapIndex);
    
    // phase 2: point lights
    //for(int i = 0; i < NR_POINT_LIGHTS; i++)
    //    result += CalcPointLight(pointLights[i], norm, FragPos, viewDir);    
    // phase 3: spot light
    //result += CalcSpotLight(spotLight, norm, FragPos, viewDir);    
    outColor = vec4(result.xyz, 1.0);
}


