#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

layout(location = 0) out vec2 out_uv;

void main()
{
    if (gl_VertexIndex == 0)
        gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);
    else if (gl_VertexIndex == 1)
        gl_Position = vec4(-1.0, 3.0, 0.0, 1.0);
    else
        gl_Position = vec4(3.0, -1.0, 0.0, 1.0);

    out_uv = gl_Position.xy * 0.5 + 0.5;
}
