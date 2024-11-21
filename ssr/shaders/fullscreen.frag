#version 460 core

#include "shade.glsl"

const vec3 noReflection = vec3(0.0f);

// See state::SSRmode for specification
const uint ssrDisabledMode = 0;
const uint ssrMixMode = 1;
const uint ssrUvMapMode = 2;
const uint ssrHeatmapMode = 3;
const uint ssrReflectionMapMode = 4;

// See state::SSRTraversalScheme for specification
const uint ssrVcsTraversalScheme = 1;
const uint ssrDDATraversalScheme = 2;

// See state::ShadingDetails for specification
const uint fresnelModulation = 0x04;
const uint environmentMapping = 0x08;

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

layout(std140, set = 1, binding = 0) uniform ShadeUniforms {
    Shade shade;
} shadeUniforms;

layout(set = 2, binding = 0) uniform sampler2D gDepth;
layout(set = 2, binding = 1) uniform sampler2D gNormal;
layout(set = 2, binding = 2) uniform sampler2D gBaseColour;
layout(set = 2, binding = 3) uniform sampler2D gSurface;

layout(std140, set = 3, binding = 0) uniform SSR {
    uint mode;
    float reflectivityThreshold;
    uint traversalScheme;
    uint maxSteps;
    float stride;
    uint binaryRefinementSteps;
    float thickness;
} ssr;

layout(set = 4, binding = 0) uniform samplerCube environmentMap;

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 colour;

vec3 reconstructPositionVcs(vec2 uv, float depth) {
    // Compute NDC
    ivec2 screenSize = textureSize(gDepth, 0);

    // Clip-space coordinates (z = depth, w = 1.0)
    vec4 position_ccs = vec4(uv * 2.0f - 1.0f, depth, 1.0);

    // Reconstruct view-space position by multiplying with the inverse projection matrix
    vec4 position_vcs = scene.iP * position_ccs;

    // Perform perspective division to get the final view-space position
    position_vcs /= position_vcs.w;

    return position_vcs.xyz;
}

bool intersectsDepthBuffer(float rayZMin, float rayZMax, float sceneDepth) {
    return (sceneDepth <= rayZMin) && (rayZMax <= (sceneDepth + ssr.thickness));
}

void refineTrace(vec3 direction_vcs, inout vec3 hit_vcs, inout vec2 hit_scs) {
    Camera camera = shadeUniforms.shade.camera;

    // Either 1 or (-1) used to flip the stride
    float strideDirection = -1.0f;
    vec3 stride_vcs = ssr.stride * direction_vcs;

    // Binary Search Refinement
    // If ssr.binaryRefinementSteps == 0, it simply skips loop and returns non-refined hit colour
    for (uint step = 0; step < ssr.binaryRefinementSteps; ++step) {
        stride_vcs *= (strideDirection * 0.5f);
        vec3 mid_vcs = hit_vcs + stride_vcs;
        vec4 mid_ccs = scene.WP * vec4(mid_vcs, 1.0f);
        vec2 mid_scs = mid_ccs.xy / mid_ccs.w;

        float depth = texelFetch(gDepth, ivec2(mid_scs), 0).r;

        if (depth == 0.0f) {
            // Sample texture out of bounds, skip this step
            continue;
        }

        float sceneDepth = lineariseDepth(camera, depth);
        float midDepth = mid_ccs.w;

        if (intersectsDepthBuffer(midDepth, midDepth, sceneDepth)) {
            hit_vcs = mid_vcs;
            hit_scs = mid_scs;
            // Search in the opposite direction
            strideDirection *= -1.0f;
        }
    }
}

bool traceRayVcs(vec3 origin_vcs, vec3 direction_vcs,  out uint stepsTaken, out vec3 hit_vcs, out vec2 hit_scs) {
    Camera camera = shadeUniforms.shade.camera;

    vec3 march_vcs = origin_vcs;
    vec3 stride_vcs = ssr.stride * direction_vcs;
    float sceneDepth = camera.far;

    for (uint step = 0; step < ssr.maxSteps && sceneDepth != 0.0f; ++step) {
        march_vcs += stride_vcs;
        vec4 march_ccs = scene.WP * vec4(march_vcs, 1.0f);
        vec2 march_scs = march_ccs.xy / march_ccs.w;

        float marchDepth = march_ccs.w;
        sceneDepth = lineariseDepth(camera, texelFetch(gDepth, ivec2(march_scs), 0).r);

        if (intersectsDepthBuffer(marchDepth, marchDepth, sceneDepth)) {
            hit_vcs = march_vcs;
            hit_scs = march_scs;
            stepsTaken = step;
            refineTrace(direction_vcs, hit_vcs, hit_scs);
            return true;
        }
    }

    return false;
}

float distanceSquared(vec2 a, vec2 b) {
    a -= b;
    return dot(a, a);
}

// Based on the work of Morgan McGuire and Michael Mara at Williams College 2014:
// https://www.jcgt.org/published/0003/04/04/
//
// Released as open source under the BSD 2-Clause License
// http://opensource.org/licenses/BSD-2-Clause
//
// Copyright (c) 2014, Morgan McGuire and Michael Mara
// All rights reserved.
//
// From McGuire and Mara, Efficient GPU Screen-Space Ray Tracing,
// Journal of Computer Graphics Techniques, 2014
//
// This software is open source under the "BSD 2-clause license":
//
// Redistribution and use in source and binary forms, with or
// without modification, are permitted provided that the following
// conditions are met:
//
// 1. Redistributions of source code must retain the above
// copyright notice, this list of conditions and the following
// disclaimer.
//
// 2. Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following
// disclaimer in the documentation and/or other materials provided
// with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
// CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
// USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
// AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

// Includes contributions from Will Pearce's blog entry from 2015:
// https://roar11.com/2015/07/screen-space-glossy-reflections/
bool traceRayDDA(vec3 origin_vcs, vec3 direction_vcs, out uint stepsTaken, out vec3 hit_vcs, out vec2 hit_scs) {
    Camera camera = shadeUniforms.shade.camera;

    // Clip to the near plane
    float rayLength = ((origin_vcs.z + direction_vcs.z * ssr.maxSteps) < camera.near) ?
        (camera.near - origin_vcs.z) / direction_vcs.z : ssr.maxSteps;
    vec3 end_vcs = origin_vcs + direction_vcs * rayLength;

    // Project into window clip-space
    vec4 H0 = scene.WP * vec4(origin_vcs, 1.0f);
    vec4 H1 = scene.WP * vec4(end_vcs, 1.0f);
    float k0 = 1.0f / H0.w;
    float k1 = 1.0f / H1.w;

    // The interpolated homogeneous version of the view-space points
    vec3 Q0 = vec3(origin_vcs.xy, -origin_vcs.z) * k0;
    vec3 Q1 = end_vcs * k1;

    // Screen-space endpoints
    vec2 P0 = H0.xy * k0;
    vec2 P1 = H1.xy * k1;

    // If the line is degenerate, make it cover at least one pixel
    // to avoid handling zero-pixel extent as a special case later
    P1 += vec2(distanceSquared(P0, P1) < 0.01f ? 0.01f : 0.0f);
    vec2 delta = P1 - P0;

    // Permute so that the primary iteration is in x
    // Collapses all quadrant-specific DDA cases later
    bool permute = false;
    if (abs(delta.x) < abs(delta.y)) {
        // This is a more-vertical line
        permute = true;
        delta = delta.yx;
        P0 = P0.yx;
        P1 = P1.yx;
    }

    float stepDir = sign(delta.x);
    float invdx = stepDir / delta.x;

    // Track the derivatives of Q and k
    vec3 dQ = (Q1 - Q0) * invdx;
    float dk = (k1 - k0) * invdx;
    vec2 dP = vec2(stepDir, delta.y * invdx);

    // Construct PQk and dPQk
    vec4 PQk = vec4(P0, Q0.z, k0);
    vec4 dPQk = vec4(dP, dQ.z, dk);

    // Scale derivatives by stride, at least 1 pixel
    dPQk *= 1.0f + ssr.stride;
    // Jitter starting value to avoid artifacts
    PQk += 0.1f * dPQk;

    // Adjust end condition for iteration direction
    float end = P1.x * stepDir;

    // Sufficiently far away
    float prevZMaxEstimate = origin_vcs.z;
    float rayZMin = prevZMaxEstimate, rayZMax = prevZMaxEstimate;
    float sceneDepth = rayZMax + camera.far;

    // Slide P from P0 to P1, (now-homogeneous) Q from Q0 to Q1, k from k0 to k1
    for(uint step = 0;
        ((PQk.x * stepDir) <= end) && (step < ssr.maxSteps) && (sceneDepth != 0.0f);
        ++step) {

        rayZMin = prevZMaxEstimate;
        rayZMax = (dPQk.z * 0.5f + PQk.z) / (dPQk.w * 0.5f + PQk.w);
        prevZMaxEstimate = rayZMax;

        if (rayZMin > rayZMax) {
            // Swap
            float temp = rayZMin;
            rayZMin = rayZMax;
            rayZMax = temp;
        }

        hit_scs = permute ? PQk.yx : PQk.xy;
        sceneDepth = lineariseDepth(camera, texelFetch(gDepth, ivec2(hit_scs), 0).r);

        if (intersectsDepthBuffer(rayZMin, rayZMax, sceneDepth)) {
            // Advance Q based on the number of steps
            vec3 Q = vec3(Q0.xy + dQ.xy * float(step), -PQk.z);
            hit_vcs = Q * (1.0f / PQk.w);
            stepsTaken = step;
            refineTrace(direction_vcs, hit_vcs, hit_scs);
            return true;
        }

        PQk += dPQk;
    }

    return false;
}

bool traceRay(vec3 origin_vcs, vec3 direction_vcs, out uint stepsTaken, out vec3 hit_vcs, out vec2 hit_scs) {
    switch (ssr.traversalScheme) {
        case ssrVcsTraversalScheme:
            return traceRayVcs(origin_vcs, direction_vcs, stepsTaken, hit_vcs, hit_scs);
        case ssrDDATraversalScheme:
            return traceRayDDA(origin_vcs, direction_vcs, stepsTaken, hit_vcs, hit_scs);
        default:
            return false;
    }
}

bool isReflective(vec3 R) {
    return any(greaterThan(R, vec3(ssr.reflectivityThreshold)));
}

vec3 reflectionColour(vec2 hit_uv, vec3 r_vcs) {
    vec4 hitSurface = texture(gSurface, hit_uv);
    float hitDepth = texture(gDepth, hit_uv).r;
    PBR hitPBR = lightPBR(shadeUniforms.shade,
                     texture(gNormal, hit_uv).xyz,
                     reconstructPositionVcs(hit_uv, hitDepth),
                     texture(gBaseColour, hit_uv).rgb,
                     hitSurface.r, hitSurface.g, hitSurface.b);

    return hitPBR.colour;
}

vec3 environmentColour(vec3 direction_vcs) {
    return texture(environmentMap, normalize(scene.C * vec4(-direction_vcs, 0.0f)).xyz).rgb;
}

vec3 ssrColour(vec3 normal_vcs, vec3 position_vcs, out uint stepsTaken, out vec3 hit_vcs, out vec2 hit_uv) {
    vec3 origin_vcs = position_vcs;
    vec3 direction_vcs = reflectionDirection(normal_vcs, position_vcs);
    vec2 hit_scs;
    bool hit = traceRay(origin_vcs, direction_vcs, stepsTaken, hit_vcs, hit_scs);
    hit_uv = hit ? hit_scs / textureSize(gDepth, 0) : vec2(-1.0f);
    vec3 reflectionColour = hit ? reflectionColour(hit_uv, direction_vcs) : noReflection;
    bool isEnvironmentEnabled = (shadeUniforms.shade.detailsBitfield & environmentMapping) != 0;
    vec3 environmentColour = !hit && isEnvironmentEnabled ? environmentColour(direction_vcs) : noReflection;

    return reflectionColour + environmentColour;
}

void main() {
    float depth = texture(gDepth, uv).r;
    vec3 normal_vcs = texture(gNormal, uv).xyz;
    vec3 position_vcs = reconstructPositionVcs(uv, depth);
    vec4 baseColour = texture(gBaseColour, uv);
    vec3 cMat = baseColour.rgb;
    vec4 surface = texture(gSurface, uv);
    float r = surface.r;
    float M = surface.g;
    float S = surface.b;

    // Discard non-framebuffered values
    if (baseColour.a <= 0.0f) {
        discard;
    }

    // If debug mode, return debug mode colour
    if (shadeUniforms.shade.visualisationMode != pbrMode ||
        shadeUniforms.shade.pbrTerm != allTerms) {
        colour = vec4(shade(shadeUniforms.shade, depth, normal_vcs, position_vcs, cMat, r, M, S), 1.0f);
        return;
    }

    PBR pbr = lightPBR(shadeUniforms.shade, normal_vcs, position_vcs, cMat, r, M, S);
    vec3 R = (shadeUniforms.shade.detailsBitfield & fresnelModulation) != 0 ? pbr.F : vec3(1.0f);
    bool performSSR = ssr.mode != ssrDisabledMode;
    vec3 hit_vcs;
    vec2 hit_uv = vec2(-1.0f);
    uint stepsTaken = 0;
    vec3 reflectionColour = performSSR && isReflective(R) ?
                            ssrColour(normal_vcs, position_vcs, stepsTaken, hit_vcs, hit_uv) :
                            vec3(0.0f);

    vec3 shadedColour;
    switch (ssr.mode) {
        case ssrDisabledMode:
            shadedColour = pbr.colour;
            break;
        case ssrMixMode:
            shadedColour = pbr.colour + R * reflectionColour;
            break;
        case ssrUvMapMode:
            shadedColour = vec3(hit_uv.x, 0.0f, hit_uv.y);
            break;
        case ssrHeatmapMode:
            shadedColour = vec3(stepsTaken / float(ssr.maxSteps), 0.0f, 0.0f);
            break;
        case ssrReflectionMapMode:
            shadedColour = reflectionColour;
            break;
    }

    colour = vec4(shadedColour, 1.0f);
}