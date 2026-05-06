#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

layout (constant_id = 0) const uint MAX_BOUNDS = 100;

layout(std140, set = 0, binding = 0) readonly buffer Transform
{
    mat4 mats[MAX_BOUNDS];
}transforms;

layout (location = 0) in vec4 pos;

void main()
{
    // mvp
    mat4 mvpMat = transforms.mats[gl_InstanceIndex];
    gl_Position = mvpMat * vec4(pos.xyz, 1.0);
}