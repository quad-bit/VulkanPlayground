#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

#include "Defines.h"

layout (std140, set = 0, binding = 0) uniform View
{
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
} view;

layout(std140, set = 1, binding = 0) uniform Transform
{
    mat4 mats[MAX_ENTITIES];
}transforms;

layout(push_constant) uniform PushConsts
{
    int transformIndex;
};

layout (location = 0) in vec3 pos;

void main()
{
   mat4 modelMat = transforms.mats[transformIndex];
   gl_Position = view.projection * view.view * modelMat * vec4(pos.xyz, 1.0);
}
