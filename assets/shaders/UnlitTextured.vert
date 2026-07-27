#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

layout (std140, set = 0, binding = 0) uniform View
{
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
} view;

layout(std140, set = 1, binding = 0) readonly buffer Transform
{
    mat4 mats[];
}transforms;


layout(push_constant) uniform PushConstsVert
{
    int transformIndex;
};

layout (location = 0) in vec4 pos;
layout (location = 1) in vec2 inTexCoord;

layout (location = 0) out vec2 outFragTexCoord;

void main()
{
   mat4 modelMat = transforms.mats[transformIndex];
   gl_Position = view.projection * view.view * modelMat * vec4(pos.xyz, 1.0);
   outFragTexCoord = inTexCoord;
}
