#version 460

const float alphaThreshold = 0.5f;

layout(set = 1, binding = 4) uniform sampler2D alphaMask;

layout(location = 0) in vec2 uv;

void main() {
    // Discard fragments with alpha below a threshold
    vec4 mask = texture(alphaMask, uv);
    float transparency = (mask.a < 1.0f)
        ? max(mask.a, dot(mask.rgb, vec3(0.333f)))  // RGBA - use the max of alpha and RGB brightness
        : dot(mask.rgb, vec3(0.333));               // RGB - only use RGB brightness
    if (transparency < alphaThreshold) {
        discard; // Don't write to depth buffer and terminate processing
    }

    // Writes to depth buffer automatically
}
