#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
#extension GL_GOOGLE_include_directive : enable

layout (std140, set = 0, binding = 0) uniform View
{
    mat4 view;
    mat4 projection;
    mat4 viewProjectActiveCamera;
} view;

// 36 vertices (6 faces × 2 triangles × 3 vertices)
//vec3 getCubeVertex(int id) {
//    vec3 positions[8] = vec3[](
//        vec3(-1.0, -1.0, -1.0), // 0
//        vec3( 1.0, -1.0, -1.0), // 1
//        vec3( 1.0,  1.0, -1.0), // 2
//        vec3(-1.0,  1.0, -1.0), // 3
//        vec3(-1.0, -1.0,  1.0), // 4
//        vec3( 1.0, -1.0,  1.0), // 5
//        vec3( 1.0,  1.0,  1.0), // 6
//        vec3(-1.0,  1.0,  1.0)  // 7
//    );
//    return positions[id];
//}

vec3 GetCubeVertex(int id)
{
    vec3 positions[24] = vec3[](
        vec3(-1.0, -1.0,  0.0), // 0
        vec3( 1.0, -1.0,  0.0), // 1

        vec3( 1.0, -1.0,  0.0), // 1
        vec3( 1.0,  1.0,  0.0), // 2

        vec3( 1.0,  1.0, 0.0), // 2
        vec3(-1.0,  1.0, 0.0), // 3

        vec3(-1.0,  1.0, 0.0), // 3
        vec3(-1.0, -1.0, 0.0), // 0

        vec3(-1.0, -1.0,  1.0), // 4
        vec3( 1.0, -1.0,  1.0), // 5

        vec3( 1.0, -1.0,  1.0), // 5
        vec3( 1.0,  1.0,  1.0), // 6

        vec3( 1.0,  1.0,  1.0), // 6
        vec3(-1.0,  1.0,  1.0),  // 7

        vec3(-1.0,  1.0,  1.0),  // 7
        vec3(-1.0, -1.0,  1.0), // 4

        vec3(-1.0, -1.0,  0.0), // 0
        vec3(-1.0, -1.0,  1.0), // 4

        vec3( 1.0, -1.0,  0.0), // 1
        vec3( 1.0, -1.0,  1.0), // 5

        vec3( 1.0,  1.0,  0.0), // 2
        vec3( 1.0,  1.0,  1.0), // 6

        vec3(-1.0,  1.0,  0.0), // 3
        vec3(-1.0,  1.0,  1.0)  // 7
    );

    return positions[id];
}


void main()
{
    mat4 vp = view.projection * view.view;
    mat4 inv = inverse(view.projection);
    vec4 cubePosition = inv * vec4(GetCubeVertex(gl_VertexIndex), 1.0f);
    cubePosition /= cubePosition.w;//vec4((cubePosition.xyz)/cubePosition.w, 1.0);
    cubePosition = view.viewProjectActiveCamera * inverse(view.view) * cubePosition;
    //cubePosition = view.viewProjectActiveCamera * cubePosition;
    
    gl_Position = cubePosition;
}