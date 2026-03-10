#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

layout (std140, set = 0, binding = 0) uniform View
{
    mat4 view;
    mat4 projection;
    vec3 cameraPos;
} view;

layout(std140, set = 1, binding = 0) uniform Transform
{
    mat4 model;
}transform;

layout (location = 0) in vec4 pos;
layout (location = 1) in vec2 inTexCoord;

layout (location = 0) out vec2 outFragTexCoord;

void main()
{
   gl_Position = view.projection * view.view * transform.model * vec4(pos.xyz, 1.0);
   outFragTexCoord = inTexCoord;
}
