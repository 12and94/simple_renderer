#version 450

layout(set = 0, binding = 0) uniform sampler2D uBaseColorTex;
layout(set = 0, binding = 1) uniform sampler2D uNormalTex;
layout(set = 0, binding = 2) uniform sampler2D uMetallicRoughnessTex;
layout(set = 0, binding = 3) uniform sampler2D uEmissiveTex;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 baseColor;
    vec4 emissiveFactor;
    vec4 materialParams;
} pc;

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inTangent;
layout(location = 2) in vec3 inBitangent;
layout(location = 3) in vec2 inUv0;

layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return nom / max(denom, 0.0001);
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / max(denom, 0.0001);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec4 baseTex = texture(uBaseColorTex, inUv0);
    vec3 albedo = pc.baseColor.rgb * baseTex.rgb;
    float alpha = pc.baseColor.a * baseTex.a;

    vec3 tangentNormal = texture(uNormalTex, inUv0).xyz * 2.0 - 1.0;
    tangentNormal.xy *= pc.materialParams.z;
    mat3 tbn = mat3(normalize(inTangent), normalize(inBitangent), normalize(inNormal));
    vec3 N = normalize(tbn * tangentNormal);

    vec4 mrTex = texture(uMetallicRoughnessTex, inUv0);
    float metallic = clamp(pc.materialParams.x * mrTex.b, 0.0, 1.0);
    float roughness = clamp(pc.materialParams.y * mrTex.g, 0.045, 1.0);

    vec3 V = normalize(vec3(0.0, 0.0, 1.0));
    vec3 L = normalize(vec3(0.5, 0.9, 0.25));
    vec3 H = normalize(V + L);

    float NDF = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    float NdotL = max(dot(N, L), 0.0);

    vec3 radiance = vec3(3.0);
    vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

    vec3 ambient = albedo * 0.03;
    vec3 emissive = texture(uEmissiveTex, inUv0).rgb * pc.emissiveFactor.rgb;
    vec3 color = ambient + Lo + emissive;
    outColor = vec4(color, alpha);
}
