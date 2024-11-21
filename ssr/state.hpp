#pragma once

#include <glm/gtx/transform.hpp>

#include "config.hpp"
#include "playback.hpp"

namespace state {
    enum class InputState {
        forward,
        backward,
        strafeLeft,
        strafeRight,
        levitate,
        sink,
        fast,
        slow,
        mousing,
        max
    };

    /*
     * pbr = 1 - PBR shading (default)
     * linearisedDepth = 2 - Position as RGB
     * normal = 3 - Normal abs
     * position = 4 - Position as RGB
     * viewDirection = 5 - Direction of fragment to camera
     * lightDirection = 6 - Direction of fragment to light
     * roughness = 7 - Roughness grayscale
     * metalness = 8 - Metalness grayscale
     * reflectionDirection = 9 - Reflection direction as RGB
     * base = 0 - Base Colour
     */
    enum class VisualisationMode {
        pbr = 1,
        linearisedDepth = 2,
        normal = 3,
        position = 4,
        viewDirection = 5,
        lightDirection = 6,
        roughness = 7,
        metalness = 8,
        reflectionDirection = 9,
        base = 0
    };

    /*
     * PBR terms only visible when VisualisationMode.pbr == state.visualisationMode
     *
     * all = 1 - Full PBR equation (default)
     * ambient = 2 - Ambient term (L_ambient)
     * diffuse = 3 - Diffuse term (L_diffuse)
     * distribution = 4 - Normal distribution term (D)
     * fresnel = 5 - Fresnel term (F)
     * geometry = 6 - Geometry attenuation term (G)
     * specular = 7 - Specular PBR term ((D * F * G) / (4 * n.v * n.l))
     * brdf = 8 - Specular + Diffuse
     */
    enum class PBRTerm {
        all = 1,
        ambient = 2,
        diffuse = 3,
        distribution = 4,
        fresnel = 5,
        geometry = 6,
        specular = 7,
        brdf = 8
    };

    /*
     * Allows toggling different shading detailing effects.
     * Represented as bit field to allow managing independently.
     *
     * none = 0x00 - No details enabled
     * normalMapping = 0x01 - Toggles normal mapping
     * shadows = 0x02 - Toggles shadows visibility, note that shadow mapping is performed anyway
     * fresnelModulation = 0x04 - Toggles Fresnel modulation for reflectivity
     * environmentMapping = 0x08 - Toggle environment mapping as reflections fallback
     */
    enum class ShadingDetails : std::uint8_t {
        none = 0x00,
        normalMapping = 0x01,
        shadows = 0x02,
        fresnelModulation = 0x04,
        environmentMapping = 0x08
    };

    /*
     * Set of SSR display modes.
     *
     * disabled = 0 - No reflections
     * reflectance = 1 - Show reflectance colour (i.e.: actual reflections)
     * uvMap = 2 - Display UV coordinate hit colour
     * heatmap = 3 - Display steps take to find reflection in the R component,
     * reflectionMap = 4 - Display only reflected pixels
     */
    enum class SSRMode {
        disabled = 0,
        reflectance = 1,
        uvMap = 2,
        heatmap = 3,
        reflectionMap = 4
    };

    /*
     * SSR traversal scheme.
     *
     * vcs = 1 - View-space Ray Marching
     * dda = 2 - Screen-space Perspective-correct DDA
     */
    enum class SSRTraversalScheme {
        vcs = 1,
        dda = 2
    };

    struct State {
        // Input state
        bool inputMap[static_cast<std::size_t>(InputState::max)] = {};

        float mouseX = 0.f, mouseY = 0.f;
        float previousX = 0.f, previousY = 0.f;

        bool wasMousing = false;

        // Camera state
        glm::mat4 camera = glm::translate(cfg::cameraInitialPosition) * cfg::cameraInitialRotation;
        glm::mat4 view = glm::inverse(camera);

        // Light state
        glm::vec3 lightPosition = cfg::initialLightPosition;
        glm::vec3 lightLookAt = cfg::initialLightLookDirection;

        // Show full PBR equation by default
        VisualisationMode visualisationMode = VisualisationMode::pbr;
        PBRTerm pbrTerm = PBRTerm::all;

        // Enable Normal Mapping and Fresnel Modulation by default
        std::uint8_t shadingDetailsMask = static_cast<std::uint8_t>(ShadingDetails::normalMapping) |
                                          static_cast<std::uint8_t>(ShadingDetails::fresnelModulation);

        // SSR config values
        SSRMode ssrMode = SSRMode::reflectance;
        SSRTraversalScheme ssrTraversalScheme = SSRTraversalScheme::vcs;
        // Discard dielectrics by default
        float reflectivityThreshold = 0.05f;
        std::uint32_t ssrMaxSteps = 500;
        float ssrStride = 0.5f;
        std::uint32_t ssrBinaryRefinementSteps = 0;
        float ssrThickness = cfg::cameraFar;

        // Take screenshot of current frame, reset after frame ends
        bool takeFrameScreenshot = false;

        // Benchmarking & Performance properties
        std::uint32_t totalBenchmarkFrames = 1000;
        // By default set it to finished
        std::uint32_t currentBenchmarkFrame = totalBenchmarkFrames;
        playback::Playback* playback = nullptr;

        bool performing_benchmarks() const;

        bool start_benchmark();
    };

    void update_state(State& state, float elapsedTime);
}
