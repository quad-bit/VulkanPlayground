#version 460
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec3 v_WorldPos;
layout(location = 1) in vec3 v_Normal;
layout(location = 2) in vec2 v_TexCoord;
layout(location = 3) in vec4 v_ScreenPos; // Proportional screen-space coordinates

layout(location = 0) out vec4 o_FragColor;

// Bindings
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 proj;
    vec3 viewPos;
} camera;

layout(set = 1, binding = 0) uniform MaterialVolume {
    vec3  attenuationColor;
    float attenuationDistance; // Distance at which attenuation color is fully reached
    float transmissionFactor;
    float ior;                 // Index of refraction
} volume;

layout(set = 2, binding = 0) sampler2D sceneColorTex;   // Opaque background pass
layout(set = 2, binding = 1) sampler2D backDepthTex;    // Transmissive object backfaces depth map
layout(set = 2, binding = 2) sampler2D sceneDepthTex;   // Opaque background depth map

// Computes light attenuation through volume using Beer-Lambert Law
vec3 SampleVolumeAbsorption(vec3 baseColor, float distance, vec3 absorbColor, float absorbDist) {
    if (absorbDist == 0.0 || absorbDist == 1.0/0.0) return baseColor;
    // Beer-Lambert Law: T = e^(-extinction * distance)
    vec3 extinction = -log(clamp(absorbColor, vec3(0.0001), vec3(1.0))) / absorbDist;
    return exp(-extinction * distance);
}

void main() {
    // 1. Calculate Screen Coordinates
    vec2 screenUV = (v_ScreenPos.xy / v_ScreenPos.w) * 0.5 + 0.5;
    
    vec3 viewDir = normalize(v_WorldPos - camera.viewPos);
    vec3 normal  = normalize(v_Normal);

    // 2. Calculate Screen-Space Refraction Vector
    float eta = 1.0 / volume.ior; // Air to Medium
    vec3 refractDir = refract(viewDir, normal, eta);

    // Offset the lookup coordinates based on refraction vector and surface orientation
    vec2 refractedUV = screenUV + refractDir.xy * 0.05; 

    // 3. Determine Volumetric Thickness (Distance)
    // Read the depth from the backface pass
    float frontDepth = gl_FragCoord.z;
    float backDepth  = texture(backDepthTex, screenUV).r;
    
    // Ensure we don't bleed into background geometry behind the object
    float opaqueDepth = texture(sceneDepthTex, refractedUV).r;
    float finalBackDepth = min(backDepth, opaqueDepth);

    // Convert non-linear Vulkan depth differences to linear world space distance
    // (Simplified linear scale approximation for brevity)
    float linearThickness = max(0.0, (finalBackDepth - frontDepth) * 100.0); 

    // 4. Sample and Filter Background
    vec3 transmittedLight = texture(sceneColorTex, refractedUV).rgb;

    // 5. Apply Transmission Volume Absorption
    vec3 volumeTargetColor = volume.attenuationColor;
    vec3 attenuatedColor = SampleVolumeAbsorption(transmittedLight, linearThickness, volumeTargetColor, volume.attenuationDistance);

    // 6. Combine with Surface Reflection (Fresnel)
    float F0 = pow((1.0 - volume.ior) / (1.0 + volume.ior), 2.0);
    float fresnel = F0 + (1.0 - F0) * pow(1.0 - max(dot(-viewDir, normal), 0.0), 5.0);

    // Mix reflected surface light (e.g., standard specular PBR) with transmitted volume light
    vec3 surfaceSpecular = vec3(1.0) * fresnel; // Placeholder for actual specular equation
    
    vec3 finalColor = mix(attenuatedColor, surfaceSpecular, fresnel);
    
    o_FragColor = vec4(finalColor, 1.0);
}


// 1. Resource Allocations: Color & Depth Textures
// VkImage transmissionColorImage; // Holds the opaque background scene
// VkImage backfaceDepthImage;     // Depth target for the transmissive object's backfaces

// 2. Render Pass Structure Sequence:
// void RecordRenderCommands(VkCommandBuffer cmdBuffer) {
    
    // --- PASS 1: RENDER OPAQUE GEOMETRY ---
    // Render normal objects to Swapchain/Main Framebuffer
    
    // --- BARRIER: Transition Main Color & Depth to Shader Read Optimal ---
    // Transition Swapchain color image and depth image so the fragment shader can sample them

    // --- PASS 2: CAPTURE TRANSMIISIVE BACKFACES ---
    // Begin a render pass targeting 'backfaceDepthImage'
    // Bind your Transmissive Pipeline, but modify Rasterization State to cull front faces:
    // pipelineRasterizationCreateInfo.cullMode = VK_CULL_MODE_FRONT_BIT;
    // Draw transmissive meshes.

    // --- BARRIER: Transition backfaceDepthImage to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ---

    // --- PASS 3: FINAL TRANSMISSION COMPOSITING PASSTHROUGH ---
    // Resume rendering to Swapchain/Main Framebuffer
    // Bind Transmissive Pipeline with backface culling restored:
    // pipelineRasterizationCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
    // Bind 'transmissionColorImage', 'backfaceDepthImage', and 'sceneDepthTex' to the Descriptor Set
    // Draw transmissive meshes.
// }
