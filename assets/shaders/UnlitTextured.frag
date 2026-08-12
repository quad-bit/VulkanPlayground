#version 450 
#extension GL_ARB_separate_shader_objects : enable 
#extension GL_ARB_shading_language_420pack : enable 
#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) in vec2 fragTexCoord;
layout (location = 0) out vec4 outColor; 

layout(push_constant) uniform PushConstsFrag
{
    layout(offset = 4) int textureIndex;
};
layout (set = 2, binding = 0) uniform sampler2D textures[];

void main() 
{ 
   outColor = texture(textures[textureIndex], fragTexCoord); 
} 


