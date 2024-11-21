#version 460

// See glsl::SceneUniform for definition
layout(std140, set = 0, binding = 0) uniform Scene {
    mat4 V;
    mat4 P;
    mat4 VP;
    mat4 LVP;
    mat4 SLVP;
    mat4 WP;
    mat4 iP;
    mat4 C;
} scene;

layout(location = 0) in vec3 vertexPosition_wcs;
layout(location = 1) in vec2 vertexUV;
layout(location = 2) in vec3 vertexNormal_wcs;
layout(location = 3) in vec4 vertexTangent;

layout(location = 0) out vec3 position_vcs;
layout(location = 1) out vec2 uv;
layout(location = 2) out vec3 normal_vcs;
layout(location = 3) out mat3 VTBN;
layout(location = 6) out vec4 position_lcs;

void main() {
    gl_Position = scene.VP * vec4(vertexPosition_wcs, 1.0f);
    position_vcs = (scene.V * vec4(vertexPosition_wcs, 1.0f)).xyz;
    uv = vertexUV;
    normal_vcs = normalize(scene.V * vec4(vertexNormal_wcs, 0.0f)).xyz;
    vec3 vertexBitangent = vertexTangent.w * cross(vertexNormal_wcs, vertexTangent.xyz);
    VTBN = mat3(normalize(scene.V * vec4(vertexTangent.xyz, 0.0f)).xyz,
                normalize(scene.V * vec4(vertexBitangent.xyz, 0.0f)).xyz,
                normal_vcs);
    position_lcs = scene.SLVP * vec4(vertexPosition_wcs, 1.0f);
}
