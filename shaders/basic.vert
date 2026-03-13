#version 450

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 baseColor;
    vec4 emissiveFactor;
    vec4 materialParams;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inUv0;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outTangent;
layout(location = 2) out vec3 outBitangent;
layout(location = 3) out vec2 outUv0;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    vec3 n = normalize(inNormal);
    vec3 t = normalize(inTangent.xyz);
    vec3 b = normalize(cross(n, t) * inTangent.w);
    outNormal = n;
    outTangent = t;
    outBitangent = b;
    outUv0 = inUv0;
}
