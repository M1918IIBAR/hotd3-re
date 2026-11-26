#version 450

layout(set = 0, binding = 0) uniform sampler2D tex0;

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 texColor = texture(tex0, inTexCoord);
    outColor = texColor * inColor;
}
