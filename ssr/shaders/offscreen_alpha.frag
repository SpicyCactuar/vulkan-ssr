#version 460

#include "shade.glsl"

const float noShadows = 1.0f;
const float alphaThreshold = 0.5f;

// See state::ShadingDetails for specification
const uint normalMappingMask = 0x01;
const uint shadowsMask = 0x02;

layout(std140, set = 1, binding = 0) uniform ShadeUniforms {
    Shade shade;
} shadeUniforms;

layout(set = 1, binding = 1) uniform sampler2DShadow shadow;

layout(set = 2, binding = 0) uniform sampler2D baseColour;
layout(set = 2, binding = 1) uniform sampler2D roughness;
layout(set = 2, binding = 2) uniform sampler2D metalness;
layout(set = 2, binding = 3) uniform sampler2D normalMap;
layout(set = 2, binding = 4) uniform sampler2D alphaMask;

layout(std140, push_constant) uniform MaterialPushConstants {
    vec3 baseColour;
    float roughness;
    float metalness;
} materialPush;

layout(location = 0) in vec3 position_vcs;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 normal_vcs;
layout(location = 3) in mat3 VTBN;
layout(location = 6) in vec4 position_lcs;

// Write each colour attachment - Depth is written implicitly
layout(location = 0) out vec4 gNormal;
layout(location = 1) out vec4 gBaseColour;
layout(location = 2) out vec4 gSurface;

vec3 mapNormal(vec3 normal_tcs) {
    return VTBN * (normal_tcs * 2.0f - 1.0f);
}

float shadowFactor() {
    return textureProj(shadow, position_lcs);
}

void main() {
    // Discard if alpha masked
    vec4 mask = texture(alphaMask, uv);
    float transparency = (mask.a < 1.0)
        ? max(mask.a, dot(mask.rgb, vec3(0.333)))  // RGBA - use the max of alpha and RGB brightness
        : dot(mask.rgb, vec3(0.333));              // RGB - only use RGB brightness
    if (transparency < alphaThreshold) {
        discard;
    }

    // Re-orient normal if Normal Mapping is enabled
    bool normalMappingEnabled = (shadeUniforms.shade.detailsBitfield & normalMappingMask) != 0;
    vec3 fragNormal_vcs = normal_vcs;
    if (normalMappingEnabled) {
        fragNormal_vcs = mapNormal(texture(normalMap, uv).rgb);
    }

    vec3 cMat = texture(baseColour, uv).rgb * materialPush.baseColour;
    float r = texture(roughness, uv).r * materialPush.roughness;
    float M = texture(metalness, uv).r * materialPush.metalness;
    float S = (shadeUniforms.shade.detailsBitfield & shadowsMask) != 0 ? shadowFactor() : noShadows;

    gNormal = vec4(fragNormal_vcs, 0.0f);
    gBaseColour = vec4(cMat, 1.0f);
    gSurface = vec4(r, M, S, 0.0f);
}