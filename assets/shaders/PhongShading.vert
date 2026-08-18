#version 450

// std430 + UBO 
#extension GL_EXT_scalar_block_layout : enable 
// for including headers
#extension GL_GOOGLE_include_directive : enable

layout (std430, set = 0, binding = 0) uniform Scene
{
    mat4 view;
    mat4 projection;
    vec4 cameraPos;
} scene;

struct Light
{
    mat4 lightMatrix;
    vec4 posOrDir;
    vec4 ambient;
    vec4 specular;
    vec4 diffuse;
    int shadowMapIndex;
};
layout (constant_id = 0) const uint MAX_LIGHTS = 4;
layout (std430, set = 0, binding = 1) uniform LightData
{
    Light lights[MAX_LIGHTS]; // at 0 directional light, rest point light
};

layout(std430, set = 1, binding = 0) readonly buffer Transform
{
    mat4 mats[];
}transforms;

layout(push_constant) uniform PushConstsVert
{
    int transformIndex;
    int materialIndex;
    //int lightIndicies[3];
};

layout (location = 0) in vec4 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;
layout (location = 3) in vec4 inTangent;

struct OutVertex
{
    //vec4 color;
    vec2 fragTexCoord;
    vec3 normal;
    vec4 tangent;
    vec4 fragPos;
    vec4 posInLightSpace[MAX_LIGHTS];
    mat3 TBN;
};

layout (location = 0) out OutVertex outVertex;

void main()
{
    mat4 modelMat = transforms.mats[transformIndex];
    mat3 normalMat = mat3(transpose(inverse(modelMat)));

    vec3 N = normalize(normalMat * inNormal);
    // vec3 T = normalize(normalMat * inTangent.xyz);
    vec3 T = normalize(mat3(modelMat) * inTangent.xyz);
    T = normalize(T - dot(T,N) * N);
    vec3 B = normalize(cross(N,T)) * inTangent.w;

    // outVertex.TBN = transpose(mat3(T, B, N));
    outVertex.TBN = mat3(T, B, N);

    //outVertex.color = inColor;
    gl_Position = scene.projection * scene.view * modelMat * vec4(inPos.xyz, 1.0);
    outVertex.fragTexCoord = inTexCoord;
    outVertex.normal = normalMat * inNormal;
    outVertex.tangent = vec4(inTangent.xyz, 1.0);
    outVertex.fragPos = modelMat * vec4(inPos.xyz, 1.0);
    for(int i = 0; i < MAX_LIGHTS; i++)
    {
        outVertex.posInLightSpace[i] = lights[i].lightMatrix * modelMat * vec4(inPos.xyz, 1.0);
    }
}
