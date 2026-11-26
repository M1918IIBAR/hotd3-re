#version 450

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 tint;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec4 inColor;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec4 outColor;

void main() {
    // Mat4 on CPU is row-major; row-vector multiplication matches HLSL mul(v, M)
    gl_Position = vec4(inPosition, 1.0) * pc.mvp;
    // Flip Y for Vulkan NDC (top-left origin)
    gl_Position.y = -gl_Position.y;
    outNormal   = inNormal;
    outTexCoord = inTexCoord;
    outColor    = inColor * pc.tint;
}
