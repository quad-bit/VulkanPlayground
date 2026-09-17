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
    vec4 posInWorldSpace;
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
    int metallicRoughnessTextureIndex;
    int normalTextureIndex;

    // KHR_materials_transmission
    int transmissionFactor;

    // KHR_materials_volume
    float attenuationDistance;
    vec4 attenuationColor;
    vec4 baseColor;
};

layout(std430, set = 3, binding = 0) readonly buffer MaterialUniform
{
    Material materials[];
};

layout(set = 3, binding = 1) uniform sampler2D sceneColorTex;   // Opaque background pass
layout(set = 3, binding = 2) uniform sampler2D sceneDepthTex;   // Opaque background depth map
layout(set = 3, binding = 3) uniform sampler2D backDepthTex;    // Transmissive object backfaces depth map


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
    float shadow  = currentDepth - bias > closestDepth ? 0.6f : 0.04f;
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

vec4 CalculateDirectionalLightColor(Light directionalLight, vec3 normal, vec3 viewDir, vec4 diffuseComponent, int specularIndex)
{
    vec3 lightDir = normalize(-directionalLight.posOrDir.xyz);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    float shininess = 32.0f;// material.shininess
    vec3 reflectDir = reflect(lightDir, normal); //-lightDir
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    float strength = 0.5f;
    // vec3 halfWayDir = normalize(lightDir + viewDir);
    // float spec = pow(max(dot(normal, halfWayDir), 0.0), shininess);

    // combine results
    vec4 specularComponent = specularIndex > -1 ? texture(textures[specularIndex], inVertex.fragTexCoord) : vec4(1.0f);
    vec4 ambient = directionalLight.ambient * diffuseComponent;
    vec4 diffuse = directionalLight.diffuse * diff * diffuseComponent;
    vec4 specular = directionalLight.specular * spec * specularComponent;

    float shadow = ShadowCalculationDirectional(0, inVertex.normal);

    return (ambient + (1.0 - shadow) * (diffuse));// + specular));
}

vec4 CalculatePointLightColor(Light pointLight)
{
    return vec4(1.0);
}

vec3 ApplyVolumeAttenuation(vec3 color, float transmissionDistance, vec4 attenuationColor, float attenuationDistance)
{
    if(attenuationDistance == 0.0f)
        return color;

    vec3 sigma = -log(attenuationColor.xyz) / attenuationDistance;
    vec3 attenuation = exp(-sigma * transmissionDistance);
    return color.xyz * attenuation;
}

float LinearizeDepth(float depth, mat4 proj)
{
    float near = proj[3][2] / proj[2][2];
    float far = proj[3][2] / (proj[2][2] + 1.0); 
    return near * far / (far + depth * (near - far));
}

void main() 
{
    //int diffuseMapIndex = materials[materialIndex].diffuseMapIndex;
    int normalMapIndex = materials[materialIndex].normalTextureIndex;
    int specularMapIndex = -1;

    vec4 color = materials[materialIndex].baseColor;

    vec3 V = normalize(scene.cameraPos.xyz - inVertex.fragPos.xyz);
    vec3 normalFromTexture = texture(textures[normalMapIndex], inVertex.fragTexCoord).rgb;
    normalFromTexture = (normalFromTexture * 2.0 - 1.0);
    vec3 N = normalize(inVertex.TBN * normalFromTexture);

    //vec4 result = CalculateDirectionalLightColor(lights[0], N, V, color, specularMapIndex);
    
    vec2 screenSize = vec2(1920, 1200);
    // 1. Calculate Screen-Space Coordinates for background sampling
    vec2 screenUV = gl_FragCoord.xy / screenSize ;

    // 2. Refraction Ray Direction (Snell's Law)
    float eta = 1.0 / 1.5f; // Air (1.0) to Glass (1.5)
    vec3 refractedRay = normalize(refract(-V, N, eta));
    
    // 3. Approximate Transmission Distance (Thickness)
    float frontDepthRaw = gl_FragCoord.z;
    float backDepthRaw = texture(backDepthTex, screenUV).r;

    float frontLinear = LinearizeDepth(frontDepthRaw, scene.projection);
    float backLinear = LinearizeDepth(backDepthRaw, scene.projection);
    
    // Thickness is the distance light travels inside the mesh
    float thickness = max(0.0, backLinear - frontLinear);

    // 4. Refraction 
    vec3 refractedRayExit = inVertex.posInWorldSpace.xyz + refractedRay * thickness;
    vec4 ndcPos = scene.projection * scene.view * vec4(refractedRayExit.xyz, 1.0f);
    vec3 refractedCoords = vec3(ndcPos.xyz)/ndcPos.w;
    float tempZ = refractedCoords.z;
    refractedCoords.y *= -1.0f;
    refractedCoords = refractedCoords * 0.5f + vec3(0.5f);
    refractedCoords.z = tempZ;
    vec2 refractedUV = clamp(refractedCoords.xy, vec2(0.001), vec2(0.999));

    // 5. Sample Opaque Background with Roughness Blurring
    vec3 transmittedLight = texture(sceneColorTex, refractedUV).rgb;

    // 6. Volumetric Absorption (Beer's Law)
    // Approximate ray distance traveled inside the volume
    // float internalDistance = thick / max(dot(-N, refractedRay), 0.001);
    transmittedLight = ApplyVolumeAttenuation(transmittedLight, thickness, materials[materialIndex].attenuationColor, materials[materialIndex].attenuationDistance);

    // 7. Standard PBR Specular/Reflection Calculation (Omitted for brevity)
    float fresnel = 0.04 + (1.0 - 0.04) * pow(1.0 - max(dot(N, V), 0.0), 3.0);

    // 8. Composite Transmission Layer with Surface Layer
    vec3 specularReflection = vec3(0.04f);
    vec3 finalColor = mix(transmittedLight, specularReflection, fresnel);

    outColor = vec4(finalColor.xyz, 1.0);
    // vec4 result = CalculateDirectionalLightColor(lights[0], N, V, vec4(finalColor.xyz, 1.0f), specularMapIndex);
    // outColor = vec4(result.xyz, 1.0);
}
